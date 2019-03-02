# Mini Kermit Transfer Protocol

Mini Kermit packet:
| SOH  | LEN | SEQ | TYPE | DATA | CHECK | MARK |


- SOH : (1 byte) start-of-header, always 0x01
- LEN : (1 byte) length, the length that follows LEN, so length - 2
- SEQ : (1 byte) sequence number, mod 64, first value 0x00
- TYPE : (1 byte) the type of the packet
  - 'S' : Send-Init, first packet
  - 'F' : File Header
  - 'D' : Data
  - 'Z' : End of File
  - 'B' : End of Transaction
  - 'Y' : ACK
  - 'N' : NAK
  - 'E' : Error
- DATA : (0-MAXL bytes) the data in each packet
- CHECK : (2 bytes) CRC on all fields, except CHECK and MARK
- MARK : (1 byte) marks the end of a packet, defauld 0x0D

The script provided sends 3 files (file1.bin, file2.bin, file3.bin) and you can change the SPEED, DELAY. LOSS, CORRUPT percentage to simulate the network. (this is a functionality of the link emulator API)

```bash
cd ./link_emulator
make
cd ../
make
./run_experiment.sh
```
dateOfCode : Feb - 2018
