#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "lib.h"

#define HOST "127.0.0.1"
#define PORT 10000

int main(int argc, char** argv) {
    msg t, *r;
    kermit_pack kt;
    s_data st;
    init(HOST, PORT);
    int file_counter;
    int seq_no = 0; //mod 64
    int c = 0;
    int recv_count = 0;
    int timeout = 0;
    unsigned short crc = 0;
    unsigned char crc1 = 0;
    unsigned char crc2 = 0;
    char last_msg = 'A';

    //'S' packet
    memset(&st, 0, sizeof(s_data));
    st.MAXL = SIZE;
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
    kt.SOH = START;
    seq_no = 0;
    kt.SEQ = seq_no;
    kt.TYPE = 'S';
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
        printf("[sender]sent_seq_no = %d'S'\n", t.payload[2]);
    }

    recv_count = 0; // retransmision counter for timeout

    while (1) {
        // while retransmits the message a max of 3 times
        if (recv_count > 2) {

            if (PRINT_TAG) {
                printf("stop sender de la 'S'\n");
            }

            return 0;
        }

        r = receive_message_timeout(st.TIME * 1000);

        if (r == NULL) {
            recv_count++;
            send_message(&t);

            if (PRINT_TAG) {
                printf("[sender]sent_seq_no = %d(TIM)'S'\n", t.payload[2]);
            }

            last_msg = 'T';
        } else {

            seq_no = r->payload[2]; // at NAK or DEC at 'S' it retransmits
                                    // each time the packet with seq_no = 0

            if (r->payload[3] == 'N') {
                //wrong message sent, retransmit

                t.payload[2] = seq_no;
                send_message(&t);

                if (PRINT_TAG) {
                    printf("[sender]sent_seq_no = %d(NAK)'S'\n", t.payload[2]);
                }

                last_msg = 'N';

            } else if (r->payload[3] == 'E') {

                if (PRINT_TAG) {
                    printf("[sender]sent_seq_no = %d(DEC)'S'\n", t.payload[2]);
                }

                send_message(&t);
                last_msg = 'E';
            } else {

                if (PRINT_TAG) {
                    printf("[sender]recv_seq_no = %d(ACK)'S'\n", r->payload[2]);
                }

                seq_no = r->payload[2];
                timeout = r->payload[5] * 1000;
                last_msg = 'Y';
                break;
            }

        }

    }

    //for all the files received as arguments
    for (file_counter = 1; file_counter < argc; file_counter++) {
        // file oppening
        int fd = open(argv[file_counter], O_RDONLY);
        //'F' packet
        memset(&t, 0, sizeof(msg));
        memset(&kt, 0, sizeof(kermit_pack));
        kt.SOH = START;
        seq_no = (seq_no + 1) % SEQ_MOD;
        kt.SEQ = seq_no;
        kt.TYPE = 'F';
        memcpy(kt.DATA, argv[file_counter], strlen(argv[file_counter]));
        kt.LEN = strlen(kt.DATA) + 5;
        t.len = kt.LEN + 2;
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
            printf("[sender]sent_seq_no = %d'F'\n", t.payload[2]);
        }

        recv_count = 0;

        while (1) {
            if (recv_count > 2) {

                if (PRINT_TAG) {
                    printf("stop sender de la 'F'\n");
                }

                kt.SOH = START;
                seq_no = (seq_no + 1) % SEQ_MOD;
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
                t.payload[t.len - 1] = st.EOL;

                send_message(&t);
                return 0;
            }

            r = receive_message_timeout(timeout);

            if (r == NULL) {
                recv_count++;
                send_message(&t); //same msg

                if (PRINT_TAG) {
                    printf("[sender]sent_seq_no = %d(TIM)'F'\n", t.payload[2]);
                }

                last_msg = 'T';
            } else {

                if (r->payload[3] == 'N') {
                    //wrong message sent, retransmit
                    if (last_msg != 'N') {
                        seq_no = r->payload[2];
                        seq_no = (seq_no + 1) % SEQ_MOD;
                    }

                    t.payload[2] = seq_no;

                    kt.SEQ = seq_no;
                    //crc redo
                    crc = crc16_ccitt(&kt, kt.LEN - 1);
                    crc1 = (crc & 65280) >> 8;
                    t.payload[t.len - 3] = crc1;
                    crc2 = crc & 255;
                    t.payload[t.len - 2] = crc2;
                    t.payload[t.len - 1] = st.EOL;

                    send_message(&t);

                    if (PRINT_TAG) {
                        printf("[sender]sent_seq_no = %d(NAK)'F'\n", t.payload[2]);
                    }

                    last_msg = 'N';

                } else if (r->payload[3] == 'E') {
                    if (last_msg != 'E') {
                        seq_no = r->payload[2];
                        seq_no = (seq_no + 1) % SEQ_MOD;
                    }

                    t.payload[2] = seq_no;

                    kt.SEQ = seq_no;
                    //crc redo
                    crc = crc16_ccitt(&kt, kt.LEN - 1);
                    crc1 = (crc & 65280) >> 8;
                    t.payload[t.len - 3] = crc1;
                    crc2 = crc & 255;
                    t.payload[t.len - 2] = crc2;
                    t.payload[t.len - 1] = st.EOL;

                    send_message(&t);

                    if (PRINT_TAG) {
                        printf("[sender]recv_seq_no = %d(DEC)'F'\n", t.payload[2]);
                    }

                    last_msg = 'E';
                } else {

                    seq_no = r->payload[2];

                    if (PRINT_TAG) {
                        printf("[sender]recv_seq_no = %d(ACK)'F'\n", seq_no);
                    }

                    last_msg = 'E';
                    break;
                }

            }

        }

        memset(&t, 0, sizeof(msg));
        memset(&kt, 0, sizeof(kermit_pack));

        //'D' packets
        while ((c = read(fd, kt.DATA, st.MAXL)) > 0) {
            kt.SOH = START;
            seq_no = (seq_no + 1) % SEQ_MOD;
            kt.SEQ = seq_no;
            kt.TYPE = 'D';
            kt.LEN = c + 5;
            t.len = (int)kt.LEN + 2;
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
                printf("[sender]sent_seq_no = %d'D'\n", t.payload[2]);
            }

            recv_count = 0;

            while (1) {
                // while retransmits the message a max of 3 times
                if (recv_count > 2) {

                    if (PRINT_TAG) {
                        printf("stop sender de la 'D'\n");
                    }

                    kt.SOH = START;
                    seq_no = (seq_no + 1) % SEQ_MOD;
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
                    t.payload[t.len - 1] = st.EOL;

                    send_message(&t);
                    return 0;
                }

                r = receive_message_timeout(timeout);

                if (r == NULL) {
                    recv_count++;
                    send_message(&t);

                    if (PRINT_TAG) {
                        printf("[sender]sent_seq_no = %d(TIM)'D'\n", t.payload[2]);
                    }

                    last_msg = 'T';
                } else {

                    if (r->payload[3] == 'N') {
                        //wrong message sent, retransmit
                        if (last_msg != 'N') {
                            seq_no = r->payload[2];
                            seq_no = (seq_no + 1) % SEQ_MOD;
                        }

                        t.payload[2] = seq_no;

                        kt.SEQ = seq_no;
                        //crc redo
                        crc = crc16_ccitt(&kt, kt.LEN - 1);
                        crc1 = (crc & 65280) >> 8;
                        t.payload[t.len - 3] = crc1;
                        crc2 = crc & 255;
                        t.payload[t.len - 2] = crc2;
                        t.payload[t.len - 1] = st.EOL;

                        send_message(&t);

                        if (PRINT_TAG) {
                            printf("[sender]sent_seq_no = %d(NAK)'D'\n", t.payload[2]);
                        }

                        last_msg = 'N';

                    } else if (r->payload[3] == 'E') {
                        if (last_msg != 'E') {
                            seq_no = r->payload[2];
                            seq_no = (seq_no + 1) % SEQ_MOD;
                        }

                        t.payload[2] = seq_no;

                        kt.SEQ = seq_no;
                        //crc redo
                        crc = crc16_ccitt(&kt, kt.LEN - 1);
                        crc1 = (crc & 65280) >> 8;
                        t.payload[t.len - 3] = crc1;
                        crc2 = crc & 255;
                        t.payload[t.len - 2] = crc2;
                        t.payload[t.len - 1] = st.EOL;

                        send_message(&t);

                        if (PRINT_TAG) {
                            printf("[sender]sent_seq_no = %d(DEC)'D'\n", t.payload[2]);
                        }

                        last_msg = 'E';
                    } else {
                        seq_no = r->payload[2];

                        if (PRINT_TAG) {
                            printf("[sender]recv_seq_no = %d(ACK)'D'\n", r->payload[2]);
                        }

                        last_msg = 'Y';
                        break;
                    }

                }

            }

            memset(&t, 0, sizeof(msg));
            memset(&kt, 0, sizeof(kermit_pack));
        }

        //'Z' packet
        kt.SOH = START;
        seq_no = (seq_no + 1) % SEQ_MOD;
        kt.SEQ = seq_no;
        kt.TYPE = 'Z';
        kt.LEN = 5;
        t.len = kt.LEN + 2;
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
            printf("[sender]sent_seq_no = %d'Z'\n", t.payload[2]);
        }

        recv_count = 0;

        while (1) {
            if (recv_count > 2) {

                if (PRINT_TAG) {
                    printf("stop sender de la 'Z'\n");
                }

                kt.SOH = START;
                seq_no = (seq_no + 1) % SEQ_MOD;
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
                t.payload[t.len - 1] = st.EOL;

                send_message(&t);
                return 0;
            }

            // while retransmits the message a max of 3 times
            r = receive_message_timeout(timeout);

            if (r == NULL) {
                recv_count++;
                send_message(&t);

                if (PRINT_TAG) {
                    printf("[sender]sent_seq_no = %d(TIM)'Z'\n", t.payload[2]);
                }

                last_msg = 'T';
            } else {

                if (r->payload[3] == 'N') {
                    //wrong message sent, retransmit

                    if (last_msg != 'N') {
                        seq_no = r->payload[2];
                        seq_no = (seq_no + 1) % SEQ_MOD;
                    }

                    t.payload[2] = seq_no;

                    kt.SEQ = seq_no;
                    //crc redo
                    crc = crc16_ccitt(&kt, kt.LEN - 1);
                    crc1 = (crc & 65280) >> 8;
                    t.payload[t.len - 3] = crc1;
                    crc2 = crc & 255;
                    t.payload[t.len - 2] = crc2;
                    t.payload[t.len - 1] = st.EOL;

                    send_message(&t);

                    if (PRINT_TAG) {
                        printf("[sender]sent_seq_no = %d(NAK)'Z'\n", t.payload[2]);
                    }

                    last_msg = 'N';

                } else if (r->payload[3] == 'E') {
                    if (last_msg != 'E') {
                        seq_no = r->payload[2];
                        seq_no = (seq_no + 1) % SEQ_MOD;
                    }

                    t.payload[2] = seq_no;

                    kt.SEQ = seq_no;
                    //crc redo
                    crc = crc16_ccitt(&kt, kt.LEN - 1);
                    crc1 = (crc & 65280) >> 8;
                    t.payload[t.len - 3] = crc1;
                    crc2 = crc & 255;
                    t.payload[t.len - 2] = crc2;
                    t.payload[t.len - 1] = st.EOL;

                    send_message(&t);

                    if (PRINT_TAG) {
                        printf("[sender]sent_seq_no = %d(DEC)'Z'\n", t.payload[2]);
                    }

                    last_msg = 'E';
                } else {
                    seq_no = r->payload[2];

                    if (PRINT_TAG) {
                        printf("[sender]recv_seq_no = %d(ACK)'Z'\n", seq_no);
                    }

                    last_msg = 'Y';
                    break;
                }

            }

        }

    }

    //'B' packet
    kt.SOH = START;
    seq_no = (seq_no + 1) % SEQ_MOD;
    kt.SEQ = seq_no;
    kt.TYPE = 'B';
    kt.LEN = 5;
    t.len = kt.LEN + 2;
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
        printf("[sender]sent_seq_no = %d\n", t.payload[2]);
    }

    recv_count = 0;

    while (1) {
        if (recv_count > 2) {

            if (PRINT_TAG) {
                printf("stop sender de la 'B'\n");
            }

            kt.SOH = START;
            seq_no = (seq_no + 1) % SEQ_MOD;
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
            t.payload[t.len - 1] = st.EOL;

            send_message(&t);
            return 0;
        }

        r = receive_message_timeout(timeout);

        if (r == NULL) {
            recv_count++;
            send_message(&t);

            if (PRINT_TAG) {
                printf("[sender]sent_seq_no = %d(TIM)'B'\n", t.payload[2]);
            }

            last_msg = 'T';
        } else {

            if (r->payload[3] == 'N') {
                //wrong message sent, retransmit
                if (last_msg != 'N') {
                    seq_no = r->payload[2];
                    seq_no = (seq_no + 1) % SEQ_MOD;
                }

                t.payload[2] = seq_no;

                kt.SEQ = seq_no;
                //crc redo
                crc = crc16_ccitt(&kt, kt.LEN - 1);
                crc1 = (crc & 65280) >> 8;
                t.payload[t.len - 3] = crc1;
                crc2 = crc & 255;
                t.payload[t.len - 2] = crc2;
                t.payload[t.len - 1] = st.EOL;

                send_message(&t);

                if (PRINT_TAG) {
                    printf("[sender]sent_seq_no = %d(NAK)'B'\n", t.payload[2]);
                }

                last_msg = 'N';

            } else if (r->payload[3] == 'E') {
                if (last_msg != 'E') {
                    seq_no = r->payload[2];
                    seq_no = (seq_no + 1) % SEQ_MOD;
                }

                t.payload[2] = seq_no;

                kt.SEQ = seq_no;
                //crc redo
                crc = crc16_ccitt(&kt, kt.LEN - 1);
                crc1 = (crc & 65280) >> 8;
                t.payload[t.len - 3] = crc1;
                crc2 = crc & 255;
                t.payload[t.len - 2] = crc2;
                t.payload[t.len - 1] = st.EOL;

                send_message(&t);

                if (PRINT_TAG) {
                    printf("[sender]sent_seq_no = %d(DEC)'B'\n", t.payload[2]);
                }

                last_msg = 'E';
            } else {

                if (PRINT_TAG) {
                    printf("[sender]recv_seq_no = %d(ACK)'B'\n", seq_no);
                }

                last_msg = 'Y';
                break;
            }

        }

    }

    if (PRINT_TAG) {
        printf("sender done\n");
    }

    return 0;
}
