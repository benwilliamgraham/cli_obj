cli_obj: cli_obj.c
	gcc -O3  -o cli_obj $< -lm

.PHONY: clean
clean:
	rm -f cli_obj
