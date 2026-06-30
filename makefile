# OBS: Tudo o que está entre parênteses são os scripts
# que deverão ser executados no terminal

# Para compilação (make all)
all:
	gcc main.c -o programa

# Para compilação e execução (make test)
test: all
	./programa

# Estressar o sistema [em breve] (make stress)
stress:

# Para limpar arquivos compilados (make clean)
clean:
	rm -f programa