// ===================== Config Jogo =====================
#define MAX_ATTEMPTS 10

static int target_number = 0;  // número a adivinhar (aleatório em cada boot)

// ===================== Função para pegar entropia via TSC =====================
static inline unsigned int rand_seed(void) {
    unsigned int lo, hi;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return lo ^ hi;
}

// ===================== Vídeo (VGA texto) =====================
static char *vidptr = (char*)0xb8000;  // memória de vídeo (80x25)
static unsigned int current_loc = 0;   // índice em bytes (2 por char)

static inline void putc_color(char c, unsigned char color) {
    vidptr[current_loc++] = c;
    vidptr[current_loc++] = color;
}

static inline void print_color(const char *s, unsigned char color) {
    for (; *s; ++s) putc_color(*s, color);
}

static inline void newline(void) {
    current_loc = (current_loc / 160 + 1) * 160; // 80 cols * 2 bytes
}

static inline void clear_screen(void) {
    for (unsigned int i = 0; i < 80 * 25; ++i) {
        vidptr[i*2]   = ' ';
        vidptr[i*2+1] = 0x07; // cinza s/ preto
    }
    current_loc = 0;
}

static void print_dec_int(int x, unsigned char color) {
    char buf[12];
    int i = 0;
    if (x == 0) { putc_color('0', color); return; }
    if (x < 0) { putc_color('-', color); x = -x; }
    while (x > 0) { buf[i++] = '0' + (x % 10); x /= 10; }
    while (i--) putc_color(buf[i], color);
}

// ===================== Portas I/O (vindas do ASM) =====================
extern unsigned char read_port(unsigned short port);
extern void write_port(unsigned short port, unsigned char data);

// >>> mudança aqui: struct para o ponteiro da IDT e assinatura de load_idt <<<
struct IDTPointer {
    unsigned short limit;
    unsigned int   base;
} __attribute__((packed));

extern void load_idt(struct IDTPointer *idtp);
extern void keyboard_handler(void); // rótulo definido no ASM

#include "keyboard_map.h"  // tabela de scancodes -> ASCII

// ===================== IDT =====================
struct IDTEntry {
    unsigned short offset_low;
    unsigned short selector;
    unsigned char  zero;
    unsigned char  type_attr;
    unsigned short offset_high;
} __attribute__((packed));

static struct IDTEntry IDT[256];

static void idt_set_gate(int n, unsigned int handler) {
    IDT[n].offset_low  = handler & 0xFFFF;
    IDT[n].selector    = 0x08;     // código do kernel (GDT já vem do GRUB)
    IDT[n].zero        = 0;
    IDT[n].type_attr   = 0x8E;     // interrupt gate, DPL=0, P=1
    IDT[n].offset_high = (handler >> 16) & 0xFFFF;
}

static void idt_init(void) {
    idt_set_gate(0x21, (unsigned int)keyboard_handler);

    // Reprograma PIC1/PIC2
    write_port(0x20, 0x11);
    write_port(0xA0, 0x11);
    write_port(0x21, 0x20);
    write_port(0xA1, 0x28);
    write_port(0x21, 0x00);
    write_port(0xA1, 0x00);
    write_port(0x21, 0x01);
    write_port(0xA1, 0x01);

    // Mascara tudo inicialmente
    write_port(0x21, 0xFF);
    write_port(0xA1, 0xFF);

    struct IDTPointer idtp;
    idtp.limit = sizeof(IDT) - 1;
    idtp.base  = (unsigned int)IDT;
    load_idt(&idtp);
}

static inline void kb_enable_irq1(void) {
    write_port(0x21, 0xFD); // habilita apenas IRQ1
}

// ===================== Estado do jogo =====================
static int attempts = 0;
static int game_over = 0;
static int input_value = 0;
static int input_len = 0;

// ===================== UI do jogo =====================
static void ui_header(void) {
    print_color("Adivinhe o Numero (0-99)\n", 0x0B);
    print_color("Digite e pressione ENTER. BACKSPACE apaga.\n", 0x07);
    print_color("Use 'r' p/ reiniciar e 'c' p/ limpar tela.\n", 0x07);
    newline();
}

static void ui_prompt(void) {
    print_color("Tentativa ", 0x07);
    print_dec_int(attempts + 1, 0x07);
    print_color(" de ", 0x07);
    print_dec_int(MAX_ATTEMPTS, 0x07);
    print_color(" => ", 0x07);
}

static void ui_feedback(int guess, const char *msg, unsigned char color) {
    print_color("Voce digitou ", 0x07);
    print_dec_int(guess, 0x07);
    print_color(" : ", 0x07);
    print_color(msg, color);
    newline();
}

// ===================== Handler principal de teclado =====================
void keyboard_handler_main(void) {
    write_port(0x20, 0x20); // EOI PIC1

    unsigned char status = read_port(0x64);
    if (!(status & 0x01)) return;
    int sc = (int)read_port(0x60);
    if (sc & 0x80) return; // break code

    char c = keyboard_map[sc];

    // Reset com 'r'
    if (game_over && (c == 'r' || c == 'R')) {
        attempts = 0;
        game_over = 0;
        input_value = 0;
        input_len = 0;
        target_number = rand_seed() % 100;

        clear_screen();
        ui_header();
        ui_prompt();
        return;
    }

    // Limpar tela com 'c'
    if (c == 'c' || c == 'C') {
        clear_screen();
        ui_header();
        ui_prompt();
        // reimprime dígitos já digitados
        if (input_len > 0) {
            int tmp = input_value;
            char buf[3];
            int len = 0;
            while (tmp > 0) { buf[len++] = '0' + (tmp % 10); tmp /= 10; }
            while (len--) putc_color(buf[len], 0x07);
        }
        return;
    }

    if (game_over) return;

    // Entrada normal
    if (c >= '0' && c <= '9') {
        if (input_len < 2) {
            putc_color(c, 0x07);
            input_value = input_value * 10 + (c - '0');
            input_len++;
        }
    } else if (c == '\b') {
        if (input_len > 0) {
            current_loc -= 2;
            putc_color(' ', 0x07);
            current_loc -= 2;
            input_value /= 10;
            input_len--;
        }
    } else if (c == '\n') {
        if (input_len > 0) {
            attempts++;
            clear_screen(); // NOVO: limpa a tela em cada tentativa
            ui_header();

            if (input_value == target_number) {
                ui_feedback(input_value, "ACERTOU! :)", 0x0A);
                print_color("Pressione 'r' para jogar de novo.\n", 0x07);
                game_over = 1;
            } else if (attempts >= MAX_ATTEMPTS) {
                ui_feedback(input_value, "Errou.", 0x0C);
                print_color("Fim de jogo. Numero era ", 0x07);
                print_dec_int(target_number, 0x0F);
                newline();
                print_color("Pressione 'r' para jogar de novo.\n", 0x07);
                game_over = 1;
            } else if (input_value < target_number) {
                ui_feedback(input_value, "Muito BAIXO", 0x0E);
                ui_prompt();
            } else {
                ui_feedback(input_value, "Muito ALTO", 0x0E);
                ui_prompt();
            }

            input_value = 0;
            input_len = 0;
        }
    }
}

// ===================== Entrada do kernel =====================
void kmain(void) {
    clear_screen();
    target_number = rand_seed() % 100;
    ui_header();

    idt_init();
    kb_enable_irq1();
    ui_prompt();

    for (;;) __asm__ __volatile__("hlt");
}
