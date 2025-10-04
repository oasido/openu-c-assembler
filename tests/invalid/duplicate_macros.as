; duplicate_macros.as - test duplicate macro names

.entry START

mcro test_macro
    mov r1, r2
mcroend

; duplicate macro definition - should cause error
mcro test_macro
    add r3, r4
mcroend

START:  test_macro
        stop

DATA: .data 123
