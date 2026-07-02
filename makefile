# OBS: Tudo o que está entre parênteses são os scripts
# que deverão ser executados no terminal

# Para compilação (make all)
all:
	gcc teste_claude.c -o programa.exe -I"C:\Program Files\OpenSSL-Win64\include" -L"C:\Program Files\OpenSSL-Win64\lib" -lcrypto -lssl

# Para compilação e execução (make test)
test: all
	./programa

# Estressar o sistema [em breve] (make stress)
stress:

# Para limpar arquivos compilados (make clean)
clean:
	rm -f programa