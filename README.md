# Microkernel - Jogo "Adivinhe o Número"

**Vídeo explicativo: Disponível no arquivo `microkernel.mp4` na raiz do projeto**

https://github.com/user-attachments/assets/e723ffd2-d554-4169-92f4-236a9d6220a4

Este projeto é um microkernel desenvolvido em **C e Assembly** que roda diretamente no emulador **QEMU**.  
Ele exibe texto na tela, lida com interrupções do teclado e implementa um jogo interativo chamado **Adivinhe o Número**.

---

## Funcionalidades

- **Escrita em tela (modo VGA texto):**  
  O kernel escreve diretamente na memória de vídeo (`0xB8000`), sem depender de sistema operacional.

- **Leitura de teclado via interrupções:**  
  - Configuração da **IDT** e reprogramação do **PIC**.  
  - Tratamento da interrupção de teclado (IRQ1).  

- **Jogo interativo - Adivinhe o Número:**  
  - O sistema sorteia um número aleatório de **0 a 99**.  
  - O jogador tem **10 tentativas** para adivinhar.  
  - Feedback imediato:
    - **Muito BAIXO**
    - **Muito ALTO**
    - **Acertou!**
  - Funções extras:
    - Pressione **`r`** → reinicia o jogo com um novo número.  
    - Pressione **`c`** → limpa a tela, sem reiniciar o jogo.  

---

## Como compilar

No Linux (ou WSL), rode os comandos:

```bash
nasm -f elf32 kernel.asm -o kasm.o
gcc -m32 -ffreestanding -c kernel.c -o kc.o
ld -m elf_i386 -T link.ld -o kernel kasm.o kc.o
```

## Como executar

Use o QEMU para rodar o kernel:

```bash
qemu-system-i386 -kernel kernel
```

