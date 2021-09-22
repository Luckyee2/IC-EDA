#include "../raven_defs.h"
#include "cam.h"
#include "spi_io.h"
#include "i2c_io.h"


#define BCD_DIGIT0(x) (x & (uint32_t)0x000F)
#define BCD_DIGIT1(x) ((x >> 4) & (uint32_t)0x000F)

volatile uint32_t counter;
volatile uint32_t counter2 = 100;


// --------------------------------------------------------
// Firmware routines
// --------------------------------------------------------

// Copy the flash worker function to SRAM so that the SPI can be
// managed without having to read program instructions from it.

void flashio(uint32_t *data, int len, uint8_t wrencmd)
{
	uint32_t func[&flashio_worker_end - &flashio_worker_begin];

	uint32_t *src_ptr = &flashio_worker_begin;
	uint32_t *dst_ptr = func;

	while (src_ptr != &flashio_worker_end)
		*(dst_ptr++) = *(src_ptr++);

	((void(*)(uint32_t*, uint32_t, uint32_t))func)(data, len, wrencmd);
}

//--------------------------------------------------------------
// NOTE: Volatile write *only* works with command 01, making the
// above routing non-functional.  Must write all four registers
// status, config1, config2, and config3 at once.
//--------------------------------------------------------------
// (NOTE: Forces quad/ddr modes off, since function runs in single data pin mode)
// (NOTE: Also sets quad mode flag, so run this before entering quad mode)
//--------------------------------------------------------------

void set_flash_latency(uint8_t value)
{
	reg_spictrl = (reg_spictrl & ~0x007f0000) | ((value & 15) << 16);

	uint32_t buffer_wr[2] = {0x01000260, ((0x70 | value) << 24)};
	flashio(buffer_wr, 5, 0x50);
}

//--------------------------------------------------------------

void flash_led(int led, bool on)
{
    uint32_t pin = (1 << led);
    reg_gpio_ena &= ~(pin);

    if (on)
        reg_gpio_data |= pin;
    else
        reg_gpio_data &= ~(pin);

}

void putchar(char c)
{
	if (c == '\n')
		putchar('\r');
	reg_uart_data = c;
}

void send_serial(uint32_t c)
{
	reg_uart_data = c;
}

void print(const char *p)
{
	while (*p)
		putchar(*(p++));
}

void print_ln(const char *p)
{
	for (int i=0; i<20; i++)
	{
	    if (*p)
		    putchar(*(p++));
        else
		    putchar(' ');
    }
}

void clear()
{
    reg_uart_data = 0x7c;
    reg_uart_data = 0x2d;
}

void home()
{
    reg_uart_data = 254;
    reg_uart_data = 0x02;
}

void print_hex(uint32_t v, int digits)
{
	for (int i = digits - 1; i >= 0; i--) {
		char c = "0123456789abcdef"[(v >> (4*i)) & 15];
		putchar(c);
	}
}

void print_dec(uint32_t v)
{
	if (v >= 2000) {
		print("OVER");
		return;
	}
	else if (v >= 1000) { putchar('1'); v -= 1000; }
	else putchar(' ');

	if 	(v >= 900) { putchar('9'); v -= 900; }
	else if	(v >= 800) { putchar('8'); v -= 800; }
	else if	(v >= 700) { putchar('7'); v -= 700; }
	else if	(v >= 600) { putchar('6'); v -= 600; }
	else if	(v >= 500) { putchar('5'); v -= 500; }
	else if	(v >= 400) { putchar('4'); v -= 400; }
	else if	(v >= 300) { putchar('3'); v -= 300; }
	else if	(v >= 200) { putchar('2'); v -= 200; }
	else if	(v >= 100) { putchar('1'); v -= 100; }
	else putchar('0');
		
	if 	(v >= 90) { putchar('9'); v -= 90; }
	else if	(v >= 80) { putchar('8'); v -= 80; }
	else if	(v >= 70) { putchar('7'); v -= 70; }
	else if	(v >= 60) { putchar('6'); v -= 60; }
	else if	(v >= 50) { putchar('5'); v -= 50; }
	else if	(v >= 40) { putchar('4'); v -= 40; }
	else if	(v >= 30) { putchar('3'); v -= 30; }
	else if	(v >= 20) { putchar('2'); v -= 20; }
	else if	(v >= 10) { putchar('1'); v -= 10; }
	else putchar('0');

	if 	(v >= 9) { putchar('9'); v -= 9; }
	else if	(v >= 8) { putchar('8'); v -= 8; }
	else if	(v >= 7) { putchar('7'); v -= 7; }
	else if	(v >= 6) { putchar('6'); v -= 6; }
	else if	(v >= 5) { putchar('5'); v -= 5; }
	else if	(v >= 4) { putchar('4'); v -= 4; }
	else if	(v >= 3) { putchar('3'); v -= 3; }
	else if	(v >= 2) { putchar('2'); v -= 2; }
	else if	(v >= 1) { putchar('1'); v -= 1; }
	else putchar('0');
}

void print_digit(uint32_t v)
{
    v &= (uint32_t)0x000F;

    if (v == 9) { putchar('9'); }
    else if (v == 8) { putchar('8'); }
    else if (v == 7) { putchar('7'); }
    else if (v == 6) { putchar('6'); }
    else if (v == 5) { putchar('5'); }
    else if (v == 4) { putchar('4'); }
    else if (v == 3) { putchar('3'); }
    else if (v == 2) { putchar('2'); }
    else if (v == 1) { putchar('1'); }
    else if (v == 0) { putchar('0'); }
    else putchar('*');
}

char getchar_prompt(char *prompt)
{
	int32_t c = -1;

	uint32_t cycles_begin, cycles_now, cycles;
	__asm__ volatile ("rdcycle %0" : "=r"(cycles_begin));


	if (prompt)
		print(prompt);

	while (c == -1) {
		__asm__ volatile ("rdcycle %0" : "=r"(cycles_now));
		cycles = cycles_now - cycles_begin;
		if (cycles > 12000000) {
			if (prompt)
				print(prompt);
			cycles_begin = cycles_now;
		}
		c = reg_uart_data;
	}

	return c;
}

char getchar()
{
	return getchar_prompt(0);
}

// ----------------------------------------------------------------------

void cmd_read_flash_regs_print(uint32_t addr, const char *name)
{
    uint32_t buffer[2] = {0x65000000 | addr, 0x0}; //
    flashio(buffer, 6, 0);

    print("0x");
    print_hex(addr, 6);
    print(" ");
    print(name);
    print(" 0x");
    print_hex(buffer[1], 2);
    print("  ");
}

void cmd_echo()
{
	print("Return to menu by sending '!'\n\n");
	char c;
	while ((c = getchar()) != '!') {
		if (c == '\r')
		    putchar('\n');
        else
		    putchar(c);
    }
}

// ----------------------------------------------------------------------

void cmd_read_flash_regs()
{
    cmd_read_flash_regs_print(0x800000, "SR1V");
    cmd_read_flash_regs_print(0x800002, "CR1V");
    cmd_read_flash_regs_print(0x800003, "CR2V");
    cmd_read_flash_regs_print(0x800004, "CR3V");
}

void rtc_run()
{
    write_i2c_slave(RTC_I2C_ADDR, 0x00, 0x00);
}

void rtc_stop()
{
    write_i2c_slave(RTC_I2C_ADDR, 0x00, 0x10);
}

void read_rtc()
{
    uint32_t data[2];

//    data = read_i2c_slave_byte(RTC_I2C_ADDR, 0x00); // RTC DS3231

    read_i2c_slave_bytes(RTC_I2C_ADDR, 0x02, data, 3); // RTC PCF8563

    data[0] &= (uint32_t) 0x007F;
    data[1] &= (uint32_t) 0x007F;
    data[2] &= (uint32_t) 0x003F;

//    print("\r");
    print("Time = ");
    print_digit(BCD_DIGIT1(data[2]));
    print_digit(BCD_DIGIT0(data[2]));
    print(":");
    print_digit(BCD_DIGIT1(data[1]));
    print_digit(BCD_DIGIT0(data[1]));
    print(":");
    print_digit(BCD_DIGIT1(data[0]));
    print_digit(BCD_DIGIT0(data[0]));
    print("     ");
    print("\n\n");

}

void display_counter()
{
    print("counter = ");
    print_dec(counter);
    print("\n");

    print("counter2 = ");
    print_dec(counter2);
    print("\n");
}

// ----------------------------------------------------------------------
// Raven demo:  Demonstrate various capabilities of the Raven testboard.
//
// 1. Flash:  Set the flash to various speed modes
// 2. GPIO:   Drive the LEDs in a loop
// 3. UART:   Print text to the UART LCD display
// 4. RC Osc: Enable the 100MHz RC oscillator and output on a GPIO line
// 5. DAC:    Enable the DAC and ramp
// 6. ADC:    Enable both ADCs and periodically read and display the DAC
//	      and bandgap values on the UART display.
// ----------------------------------------------------------------------
// NOTE:  There are two versions of this demo.  Demo 1 runs only up to
//	  2X speed (DSPI + CRM).  The higher speeds cause some timing
//	  trouble, which may be due to reflections on the board wires
//	  for IO2 and IO3.  Not sure of the mechanism.  To keep the
//	  boards running without failure, do not use QSPI modes.

void main()
{
	uint32_t i, j, m, r, mode;
	uint32_t adcval;
	uint32_t dacval;
	uint32_t data, data2, fifo_size, length, last_data;
	char k;
	bool is_header;

	counter = 0;

	/* Note that it definitely does not work in simulation because	*/
	/* the behavioral verilog for the SPI flash does not support	*/
	/* the configuration register read/write functions.		*/

	set_flash_latency(8);
	reg_spictrl = 0x80080000;

	m = 1;

    // disable all pull-up and pull-down gpio resistors
	reg_gpio_pu = 0xffff;
	reg_gpio_pd = 0xffff;

	// NOTE: Crystal on testboard running at 12.5MHz
	// Internal clock is 8x crystal, or 100MHz
	// Divided by clkdiv is 9.6 kHz
	// So at this crystal rate, use clkdiv = 10417 for 9600 baud.

	// Set UART clock to 9600 baud
	reg_uart_clkdiv = 10417;


	// Need boot-up time for the display;  give it 4 seconds
	for (j = 0; j < 700000 * m; j++);

	// This should appear on the LCD display 4x20 characters.
    print("Starting...\n");
    i2c_init();
    spi_init();

    print("Press ENTER to continue..\n");
    while (getchar() != '\r') {}

    print("\n\n");

	int led_a = 6;
	int led_b = 7;

    // test spi with write and read from test register
    if (check_spi()) {
        print("SPI communication successful.\n");
    } else {
        print("SPI communication FAILED!\n");
    }

    // test if camera is present
    uint32_t vid, pid;
    if (check_camera(&vid, &pid)) {
        print("Camera Located (");
    } else {
        print("Camera NOT FOUND (");
    }
    print("vid = 0x");
    print_hex(vid,2);
    print(", pid = 0x");
    print_hex(pid,2);
    print(")\n");

	// reset CPLD
//	reset_cpld();

//	init_camera();
//	if (!set_JPEG_size(OV2640_320x240))
//	    print("set_JPEG_size FAILED!\n");

//    for (j = 0; j < 170000; j++); // 1 sec delay
//    set_frame_count();



	while (1) {

        k = getchar();

        counter++;
        counter2++;

        switch(k) {
            case'0':
                print("initialize camera...\n");
            	reset_cpld();
                init_camera();
                for (j = 0; j < 170000; j++); // 1 sec delay
//                set_frame_count();
                break;
            case'1':
                print("clear FIFO...\n");
                flush_fifo();
                clear_fifo_flag();
                break;
            case'2':
                print("triggering capture...\n");
                flash_led(led_a, true); flash_led(led_b, false);

                // reset FIFO and flag
                flush_fifo();
                clear_fifo_flag();

                // trigger capture
                start_capture();

                // wait for status
                i = 0;
                while (i < 100) {
                    data = read_spi_slave_byte(0x41);
                    if (data & 0x08) {
                        break;
                    }
                    i++;
                  if (i == 100)
                        print("Error - capture flag not found.\n");
                };

                flash_led(led_a, false); flash_led(led_b, true);
                break;
            case '3':
                print("reading FIFO register length...\n");
                fifo_size = read_fifo_length();
                print("   ");
                print("0x"); print_hex(fifo_size, 6);
                print("\n");
                break;
            case '4':
                print("reading registers...\n");

                data = read_spi_slave_byte(0x01);
                print("   ");
                print("0x"); print_hex(data, 2);

                data = read_spi_slave_byte(0x40);
                print("   ");
                print("0x"); print_hex(data, 2);

                data = read_spi_slave_byte(0x42);
                print("   ");
                print("0x"); print_hex(data, 2);

                data = read_spi_slave_byte(0x43);
                print("   ");
                print("0x"); print_hex(data, 2);

                data = read_spi_slave_byte(0x44);
                print("   ");
                print("0x"); print_hex(data, 2);

                print("\n");
                break;
            case '5':
                print("display FIFO data (next 80 values)...\n");

                for (i=0; i<20; i++) {
                    for (j=0; j<20; j++) {
                        data = read_spi_slave_byte(0x3D);
                        print("  ");
                        print_hex(data, 2);
                    }
                    print("\n");
                }
                break;
            case '6':
                print("resetting FIFO read pointer\n");
                reset_fifo_read_ptr();
                break;
            case '7':
                reset_fifo_read_ptr();
                fifo_size = read_fifo_length();
                data = read_spi_slave_byte(FIFO_SIZE1); send_serial(data);
                data = read_spi_slave_byte(FIFO_SIZE2); send_serial(data);
                data = read_spi_slave_byte(FIFO_SIZE3); send_serial(data);
                send_serial(0xff);
                length = fifo_size;
                is_header = false;
                data = read_spi_slave_byte(0x3D);
                length--;
                while (length--) {
                    last_data = data;
                    data = read_spi_slave_byte(0x3D);
                    if (is_header == true)
                        send_serial(data);
                    else if ((data == 0xd8) && (last_data == 0xff)) {
                        is_header = true;
                        send_serial(last_data);
                        send_serial(data);
                    }
                    if ((data == 0xd9) && (last_data == 0xff))
                        break;
                }
                print("transfer complete.\n");
                break;
            case '8':
                print("setting JPEG size to 160x120\n");
                if (!set_JPEG_size(OV2640_160x120))
	                print("set_JPEG_size FAILED!\n");
                break;
            case '9':
                print("setting JPEG size to 320x240\n");
                if (!set_JPEG_size(OV2640_320x240))
	                print("set_JPEG_size FAILED!\n");
                break;
            case 'c':
                print("read real-time clock...\n");
                read_rtc();
                break;
            case 'g':
                print("display loop counter...\n");
                display_counter();
                break;

            default:
                print("menu:\n");
                print("[0] initialize camera\n");
                print("[1] clear FIFO\n");
                print("[2] trigger capture\n");
                print("[3] read FIFO length registers\n");
                print("[4] read registers\n");
                print("[5] display FIFO data (next 80 values)\n");
                print("[6] reset FIFO read pointer\n");
                print("[7] transfer FIFO data\n");
                print("[8] set JPEG size to 160x120\n");
                print("[9] set JPEG size to 320x240\n");
                print("[c] read real-time clock\n");
                print("[g] display loop counter\n");
                print("[q] quit serial client\n");
                break;
        }

        for (j = 0; j < 170000; j++); // 1 sec delay

	}
}

