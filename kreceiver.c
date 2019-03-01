#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "lib.h"

#define HOST "127.0.0.1"
#define PORT 10001

int main(int argc, char** argv) {
    msg *r, t;
    kermit_pack kt;
    s_data st;
    init(HOST, PORT);
    int timeout = 0;
    int s_timeout = TIMEOUT; //PACKET 'S' default timeout
    FILE* fd = NULL;
    char name[128];
    char data[256];
    char eol = STD_EOL;
    int seq_no = 0; //mod 64
    unsigned short crc = 0;
    unsigned char crc1 = 0;
    unsigned char crc2 = 0;
    unsigned short crc_msg = 0;
    char done_flag = 0;
    int turns = 0;

    //PACKET 'S'
    while (turns < 3) {
        r = receive_message_timeout(s_timeout);

        if (r != NULL) {

            //crc
            crc = crc16_ccitt(&r->payload, r->len - 3); //3 = kt.CHECK + kt.MARK
            crc_msg = make_crc(r->payload[r->len - 3], r->payload[r->len - 2]);

            if (crc == crc_msg) {

                timeout = r->payload[5] * 1000; //timeout from 'S'
                seq_no = r->payload[2];

                if (PRINT_TAG) {
                    printf("[reciver]recv_seq_no = %d'S'\n", seq_no);
                }

                eol = r->payload[r->len - 1];

                //ACK special pentru 'S'

                memset(&st, 0, sizeof(s_data));
                st.MAXL = 250;
                st.TIME = TIMEOUT / 1000; //in seconds
                st.NPAD = 0;
                st.PADC = 0;
                st.EOL = 0x0d;
                st.QCTL = 0;
                st.QBIN = 0;
                st.CHKT = 0;
                st.REPT = 0;
                st.CAPA = 0;
                st.R = 0;
                memset(&t, 0, sizeof(msg));
                memset(&kt, 0, sizeof(kermit_pack));
                kt.SOH = 0x01;
                seq_no = (seq_no + 1) % SEQ_MOD;
                kt.SEQ = seq_no;
                kt.TYPE = 'Y';
                memcpy(kt.DATA, &st, sizeof(s_data));
                kt.LEN = sizeof(s_data) + 5; //5 = kt.SEQ + kt.TYPE + kt.CHECK + kt.MARK
                //minus 1 because after kt.LEN there are kt.DATA bytes + 5
                //and before kt.CHECK there are 4 + kt.DATA bytes
                t.len = kt.LEN + 2; //2 from berofe kt.LEN
                memcpy(&t.payload, &kt, t.len - 3);
                //crc
                crc = crc16_ccitt(&kt, kt.LEN - 1);
                crc1 = (crc & 65280) >> 8;
                t.payload[t.len - 3] = crc1;
                crc2 = crc & 255;
                t.payload[t.len - 2] = crc2;
                t.payload[t.len - 1] = st.EOL;

                send_message(&t);

                if (PRINT_TAG) {
                    printf("[reciver]sent_seq_no = %d(ACK)\n", t.payload[2]);
                }

                break;

            } else {

                // NAK for corrupt message
                kt.SOH = 0x01;
                seq_no = (seq_no) % SEQ_MOD;
                // at NAK or DEC at 'S' it retransmits
                // each time the packet with seq_no = 0
                kt.SEQ = seq_no;
                kt.TYPE = 'N';
                kt.LEN = 5;
                t.len = kt.LEN + 2;
                memcpy(&t.payload, &kt, t.len - 3);
                //crc
                crc = crc16_ccitt(&kt, kt.LEN - 1);
                crc1 = (crc & 65280) >> 8;
                t.payload[t.len - 3] = crc1;
                crc2 = crc & 255;
                t.payload[t.len - 2] = crc2;
                t.payload[t.len - 1] = eol;
                send_message(&t);

                if (PRINT_TAG) {
                    printf("[reciver]sent_seq_no = %d(NAK)\n", t.payload[2]);
                }
            }

        } else { // specian contition for packet type 'S'

            if (PRINT_TAG) {
                printf("[reciver]sent_seq_no =NULL(TIM)\n");
            }

            turns++;
            s_timeout *= 2; // increase the timeout if it is not transmited

        }
    }

    while (turns < 3) {
        //special if because I can't break the loop in the switch
        if (done_flag == 1) {

            if (PRINT_TAG) {
                printf("receiver done\n");
            }

            break;
        }

        r = receive_message_timeout(timeout);

        //if to check if message got here witin timeout
        if (r != NULL && (r->payload[2] == (seq_no + 1) % SEQ_MOD ||
                          r->payload[3] == 'E')) {
            // condition for not NULL messages
            // and not delayed or error messages from
            // sender(if the receiver stopped)

            //crc
            crc = crc16_ccitt(&r->payload, r->len - 3); //3 = kt.CHECK + kt.MARK
            crc_msg = make_crc(r->payload[r->len - 3], r->payload[r->len - 2]);

            turns = 0; // each time the message is not NULL
            // the stop condition is brought back to 0

            // CRC check and expected packet check
            if (crc == crc_msg) {

                seq_no = r->payload[2];

                if (PRINT_TAG) {
                    printf("[reciver]recv_seq_no = %d", seq_no);
                }

                //ACK for right message
                kt.SOH = 0x01;
                seq_no = (seq_no + 1) % SEQ_MOD;
                kt.SEQ = seq_no;
                kt.TYPE = 'Y';
                kt.LEN = 5;
                t.len = kt.LEN + 2;
                memcpy(&t.payload, &kt, t.len - 3);
                //crc
                crc = crc16_ccitt(&kt, kt.LEN - 1);
                crc1 = (crc & 65280) >> 8;
                t.payload[t.len - 3] = crc1;
                crc2 = crc & 255;
                t.payload[t.len - 2] = crc2;
                t.payload[t.len - 1] = eol;

                memset(data, 0, 256);
                //data array to save the data in the sent packet
                memcpy(&data, &r->payload[4], r->len - JUST_DATA);

                //switch to determine what kind of packet got here
                switch (r->payload[3]) {
                case 'F': {

                    if (PRINT_TAG) {
                        printf("'F'\n");
                    }

                    fd = NULL;
                    //makes the output file name
                    memset(name, 0, 128);
                    strcpy(name, "recv_");
                    strcat(name, data);
                    //opens the file
                    fd = fopen(name, "wb");
                    send_message(&t);

                    if (PRINT_TAG) {
                        printf("[reciver]sent_seq_no = %d(ACK)\n", t.payload[2]);
                    }

                    continue;
                }
                case 'D': {

                    if (PRINT_TAG) {
                        printf("'D'\n");
                    }

                    fwrite(data, 1, r->len - JUST_DATA, fd);
                    send_message(&t);

                    if (PRINT_TAG) {
                        printf("[reciver]sent_seq_no = %d(ACK)\n", t.payload[2]);
                    }

                    continue;
                }
                case 'Z': {

                    if (PRINT_TAG) {
                        printf("'Z'\n");
                    }

                    fclose(fd);
                    send_message(&t);

                    if (PRINT_TAG) {
                        printf("[reciver]sent_seq_no = %d(ACK)\n", t.payload[2]);
                    }

                    continue;
                }
                case 'B': {

                    if (PRINT_TAG) {
                        printf("'B'\n");
                    }

                    done_flag = 1;
                    send_message(&t);

                    if (PRINT_TAG) {
                        printf("[reciver]sent_seq_no = %d(ACK)\n", t.payload[2]);
                    }

                    continue;
                }
                case 'E': {

                    if (PRINT_TAG) {
                        printf("\n\nreceiver stopped\n\n");
                    }

                    return 0;
                }
                }

            } else {

                //NAK for corrupt message
                kt.SOH = 0x01;
                seq_no = (seq_no + 2) % SEQ_MOD;
                kt.SEQ = seq_no;
                kt.TYPE = 'N';
                kt.LEN = 5;
                t.len = kt.LEN + 2;
                memcpy(&t.payload, &kt, t.len - 3);
                //crc
                crc = crc16_ccitt(&kt, kt.LEN - 1);
                crc1 = (crc & 65280) >> 8;
                t.payload[t.len - 3] = crc1;
                crc2 = crc & 255;
                t.payload[t.len - 2] = crc2;
                t.payload[t.len - 1] = eol;
                send_message(&t);

                if (PRINT_TAG) {
                    printf("[reciver]sent_seq_no = %d(NAK)\n", t.payload[2]);
                }

            }

        } else {

            // there is no NAK for a timeout

            if (r == NULL) {

                if (PRINT_TAG) {
                    printf("[reciver]sent_seq_no =NULL(TIM)\n");
                }

                turns++;

            }

            if (r != NULL) {
                //condition for delayed messages

                kt.SOH = 0x01;
                seq_no = (seq_no) % SEQ_MOD;
                kt.SEQ = seq_no;
                kt.TYPE = 'E';
                kt.LEN = 5;
                t.len = kt.LEN + 2;
                memcpy(&t.payload, &kt, t.len - 3);
                //crc
                crc = crc16_ccitt(&kt, kt.LEN - 1);
                crc1 = (crc & 65280) >> 8;
                t.payload[t.len - 3] = crc1;
                crc2 = crc & 255;
                t.payload[t.len - 2] = crc2;
                t.payload[t.len - 1] = eol;
                send_message(&t);

                if (PRINT_TAG) {
                    printf("[reciver]sent_seq_no = %d(DEC), expected %d got %d\n",
                           t.payload[2],
                           (seq_no + 1) % SEQ_MOD,
                           r->payload[2]);
                }

            }

        }

    }

    return 0;
}
