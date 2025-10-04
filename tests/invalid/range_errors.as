; range_errors.as - test values out of range

.entry START

START:  mov #1000, r1      ; immediate value out of range (max 511)
        add #-1000, r2     ; immediate value out of range (min -512)
        sub r3, r4
        prn #32767         ; way out of range
        stop

; Data values out of range
DATA1: .data 1000, -1000, 32767   ; values out of 10-bit range
DATA2: .data 512, -513            ; just outside valid range

; Matrix with out of range values
MATRIX: .mat [2][2] 600, -600, 1000, -1000
