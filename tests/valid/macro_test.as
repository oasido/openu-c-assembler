; macro_test.as - test macro functionality

.entry START

mcro m1
    mov r1, r2
    add #10, r1
mcroend

mcro m2
    prn #-1
    stop
mcroend

START:  m1
        lea DATAARR[r0][r1], r3
        m2

DATAARR: .mat [2][2] 10, 20, 30, 40
VALUE:  .data 123
