assembler:
	gcc -ansi -Wall -pedantic \
		./src/assembler.c ./src/preprocessor.c ./src/helpers.c  ./src/data_image.c ./src/instruction_image.c  ./src/instruction_utils.c ./src/first_pass.c ./src/second_pass.c ./src/symbol_table.c \
		-o assembler
clean:
	rm -f assembler
	rm -f input.am
