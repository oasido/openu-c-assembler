; undefined_symbols.as - test undefined symbols and labels

.entry undefinedSymbol
.extern validExtern

START:  mov r1, undefinedLabel
        add #5, r2
        jmp missingLabel
        lea NONEXISTENT[r1][r2], r3
        sub ANOTHERMISSING, r4
        prn validExtern
        stop

; some valid
validData: .data 42
validString: .string "test"
