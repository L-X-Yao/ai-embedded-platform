// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_rc522.h"
#include "rc522.h"

static const rc522_spi_t hw_spi = {
    .cs_low   = hw_model_cs_low,
    .cs_high  = hw_model_cs_high,
    .xfer     = hw_model_xfer,
    .delay_ms = hw_model_delay_ms,
    .ctx      = NULL,
};

static rc522_t dev;

/* The host side of section 8.1.2, written here so that these tests reach the
 * model only through what a real host can do. */
static uint8_t spi_read(uint8_t reg)
{
    hw_model_cs_low(NULL);
    hw_model_xfer(NULL, (uint8_t)(0x80u | (uint8_t)(reg << 1)));
    uint8_t val = hw_model_xfer(NULL, 0x00);
    hw_model_cs_high(NULL);
    return val;
}

static void spi_write(uint8_t reg, uint8_t val)
{
    hw_model_cs_low(NULL);
    hw_model_xfer(NULL, (uint8_t)(reg << 1));
    hw_model_xfer(NULL, val);
    hw_model_cs_high(NULL);
}

/* Registers with a fixed reset value, from the tables of section 9.3 of data
 * sheet rev 3.9, pp.38-69. The xxh ones are left out. */
static const struct { uint8_t reg; uint8_t val; } reset_values[] = {
    {0x00, 0x00}, {0x01, 0x20}, {0x02, 0x80}, {0x03, 0x00}, {0x04, 0x14},
    {0x06, 0x00}, {0x07, 0x21}, {0x08, 0x00}, {0x0A, 0x00}, {0x0B, 0x08},
    {0x0C, 0x10}, {0x0D, 0x00}, {0x0F, 0x00}, {0x10, 0x00}, {0x11, 0x3F},
    {0x12, 0x00}, {0x13, 0x00}, {0x14, 0x80}, {0x15, 0x00}, {0x16, 0x10},
    {0x17, 0x84}, {0x18, 0x84}, {0x19, 0x4D}, {0x1A, 0x00}, {0x1B, 0x00},
    {0x1C, 0x62}, {0x1D, 0x00}, {0x1E, 0x00}, {0x1F, 0xEB}, {0x20, 0x00},
    {0x21, 0xFF}, {0x22, 0xFF}, {0x23, 0x88}, {0x24, 0x26}, {0x25, 0x87},
    {0x26, 0x48}, {0x27, 0x88}, {0x28, 0x20}, {0x29, 0x20}, {0x2A, 0x00},
    {0x2B, 0x00}, {0x2C, 0x00}, {0x2D, 0x00}, {0x30, 0x00}, {0x31, 0x00},
    {0x32, 0x00}, {0x33, 0x80}, {0x34, 0x00}, {0x36, 0x40}, {0x38, 0x00},
    {0x3C, 0xFF}, {0x3D, 0x00}, {0x3E, 0x03}, {0x3F, 0x00},
};

/* Bring the model to the state rc522_init leaves it in, by hand */
static void field_on_timer_auto(void)
{
    hw_model_reset();
    spi_write(0x01, 0x00);  /* Idle, which also clears RcvOff */
    spi_write(0x2A, 0x8D);  /* TAuto */
    spi_write(0x2D, 0x1E);
    spi_write(0x14, 0x83);  /* Tx1RFEn, Tx2RFEn */
}

static void start_transceive(uint8_t last_bits)
{
    spi_write(0x04, 0x7F);
    spi_write(0x09, 0x26);
    spi_write(0x0D, last_bits);
    spi_write(0x01, 0x0C);
}

/* StartSend, then poll ComIrqReg as long as the exchange takes */
static void send_and_wait(void)
{
    spi_write(0x0D, 0x87);
    for (int i = 0; i < HW_AIR_POLLS; i++) (void)spi_read(0x04);
}

TEST(test_hw_init_por_defaults)
{
    hw_model_reset();
    ASSERT_EQ(rc522_init(&dev, &hw_spi), RC522_OK);
    ASSERT_EQ(dev.version, 0x92);
}

TEST(test_hw_init_v1)
{
    hw_model_reset();
    hw_regs[0x37] = RC522_VERSION_1_0;
    ASSERT_EQ(rc522_init(&dev, &hw_spi), RC522_OK);
    ASSERT_EQ(dev.version, 0x91);
}

TEST(test_hw_detect_card_with_atqa)
{
    hw_model_reset();
    rc522_init(&dev, &hw_spi);

    /* Inject ATQA into FIFO and set RxIRq */
    hw_model_inject_atqa(0x04, 0x00);

    ASSERT_EQ(rc522_detect_card(&dev), RC522_OK);
}

TEST(test_hw_read_uid)
{
    hw_model_reset();
    rc522_init(&dev, &hw_spi);

    /* Inject UID response */
    hw_model_inject_uid(0xCA, 0xFE, 0xBA, 0xBE);

    rc522_uid_t uid;
    ASSERT_EQ(rc522_read_card_uid(&dev, &uid), RC522_OK);
    ASSERT_EQ(uid.len, 4);
    ASSERT_EQ(uid.bytes[0], 0xCA);
    ASSERT_EQ(uid.bytes[1], 0xFE);
    ASSERT_EQ(uid.bytes[2], 0xBA);
    ASSERT_EQ(uid.bytes[3], 0xBE);
}

/* pp.38-69: every register starts at its documented reset value */
TEST(test_hw_power_on_values_follow_the_manual)
{
    hw_model_reset();
    for (size_t i = 0; i < sizeof(reset_values) / sizeof(reset_values[0]); i++) {
        ASSERT_EQ_HEX(spi_read(reset_values[i].reg), reset_values[i].val);
    }
}

/* p.73: "All registers are set to the reset values." */
TEST(test_hw_soft_reset_restores_every_register)
{
    hw_model_reset();
    for (size_t i = 0; i < sizeof(reset_values) / sizeof(reset_values[0]); i++) {
        uint8_t reg = reset_values[i].reg;
        if (reg != 0x01 && reg != 0x04) spi_write(reg, 0x55);
    }
    spi_write(0x01, 0x0F);
    for (size_t i = 0; i < sizeof(reset_values) / sizeof(reset_values[0]); i++) {
        ASSERT_EQ_HEX(spi_read(reset_values[i].reg), reset_values[i].val);
    }
}

TEST(test_hw_soft_reset_keeps_the_version)
{
    hw_model_reset();
    hw_regs[0x37] = 0x91;
    spi_write(0x01, 0x0F);
    ASSERT_EQ_HEX(spi_read(0x37), 0x91);
}

/* p.72: "Each transmit process must be started by setting the BitFramingReg
 * register's StartSend bit to logic 1." */
TEST(test_hw_card_answers_only_after_start_send)
{
    field_on_timer_auto();
    hw_model_inject_atqa(0x04, 0x00);
    start_transceive(0x07);

    ASSERT_EQ_HEX(spi_read(0x04) & 0x20, 0x00);
    ASSERT_EQ(spi_read(0x0A), 1);  /* still the frame to send */

    send_and_wait();
    ASSERT_EQ_HEX(spi_read(0x04) & 0x20, 0x20);
    ASSERT_EQ(spi_read(0x0A), 2);
    ASSERT_EQ_HEX(spi_read(0x09), 0x04);
    ASSERT_EQ_HEX(spi_read(0x09), 0x00);
}

/* p.46: StartSend is "only valid in combination with the Transceive command" */
TEST(test_hw_start_send_needs_transceive)
{
    field_on_timer_auto();
    hw_model_inject_atqa(0x04, 0x00);
    spi_write(0x04, 0x7F);
    spi_write(0x09, 0x26);
    send_and_wait();
    ASSERT_EQ_HEX(spi_read(0x04) & 0x60, 0x00);
    ASSERT_EQ(spi_read(0x0A), 1);
}

/* p.50: only Tx1RFEn and Tx2RFEn put the carrier on TX1 and TX2 */
TEST(test_hw_no_field_no_answer)
{
    field_on_timer_auto();
    spi_write(0x14, 0x80);
    hw_model_inject_atqa(0x04, 0x00);
    start_transceive(0x07);
    send_and_wait();

    ASSERT_EQ_HEX(spi_read(0x04) & 0x20, 0x00);
    ASSERT_EQ(spi_read(0x0A), 0);
}

TEST(test_hw_one_tx_driver_is_a_field)
{
    field_on_timer_auto();
    spi_write(0x14, 0x81);
    hw_model_inject_atqa(0x04, 0x00);
    start_transceive(0x07);
    send_and_wait();
    ASSERT_EQ_HEX(spi_read(0x04) & 0x20, 0x20);
}

/* p.38: RcvOff "analog part of the receiver is switched off". CommandReg resets
 * to 20h, so the receiver stays off until a command write clears the bit. */
TEST(test_hw_rcvoff_blocks_the_answer)
{
    field_on_timer_auto();
    hw_model_inject_atqa(0x04, 0x00);
    spi_write(0x04, 0x7F);
    spi_write(0x09, 0x26);
    spi_write(0x0D, 0x07);
    spi_write(0x01, 0x2C);
    send_and_wait();
    ASSERT_EQ_HEX(spi_read(0x04) & 0x20, 0x00);
}

/* p.61: with TAuto the timer "starts automatically at the end of the
 * transmission", so nothing can run out before StartSend */
TEST(test_hw_timer_runs_out_after_the_transmission)
{
    field_on_timer_auto();
    start_transceive(0x07);
    ASSERT_EQ_HEX(spi_read(0x04) & 0x01, 0x00);

    send_and_wait();
    ASSERT_EQ_HEX(spi_read(0x04) & 0x01, 0x01);
}

/* p.61: TAuto 0 "indicates that the timer is not influenced by the protocol" */
TEST(test_hw_timer_needs_tauto)
{
    field_on_timer_auto();
    spi_write(0x2A, 0x0D);
    start_transceive(0x07);
    send_and_wait();
    ASSERT_EQ_HEX(spi_read(0x04) & 0x01, 0x00);
}

/* p.61: the timer "stops immediately after receiving the 5th bit" */
TEST(test_hw_answer_stops_the_timer)
{
    field_on_timer_auto();
    hw_model_inject_atqa(0x04, 0x00);
    start_transceive(0x07);
    send_and_wait();
    ASSERT_EQ_HEX(spi_read(0x04) & 0x01, 0x00);
}

/* p.40: "indicates that the marked bits in the ComIrqReg register are set /
 * cleared". Bit 7 itself is write only, so it reads 0 (table 19, p.35). */
TEST(test_hw_comirq_set1_marks_bits)
{
    hw_model_reset();
    spi_write(0x04, 0x7F);
    ASSERT_EQ_HEX(spi_read(0x04), 0x00);
    spi_write(0x04, 0x81);
    ASSERT_EQ_HEX(spi_read(0x04), 0x01);
    spi_write(0x04, 0xC0);
    ASSERT_EQ_HEX(spi_read(0x04), 0x41);
    spi_write(0x04, 0x01);
    ASSERT_EQ_HEX(spi_read(0x04), 0x40);
}

/* Table 19, p.35: R bits "hold values which are determined by internal states
 * only" */
TEST(test_hw_read_only_registers_ignore_writes)
{
    static const uint8_t ro[] = {0x06, 0x07, 0x21, 0x22, 0x2E, 0x2F, 0x35,
                                 0x37, 0x3B};
    hw_model_reset();
    for (size_t i = 0; i < sizeof(ro); i++) {
        uint8_t before = spi_read(ro[i]);
        spi_write(ro[i], (uint8_t)~before);
        ASSERT_EQ_HEX(spi_read(ro[i]), before);
    }
}

/* Table 19, p.35: W bits, "Reading these register bits always returns zero." */
TEST(test_hw_start_send_reads_zero)
{
    field_on_timer_auto();
    spi_write(0x01, 0x0C);
    send_and_wait();
    ASSERT_EQ_HEX(spi_read(0x0D), 0x07);
}

/* p.44: FIFOLevel[6:0] is read only, only FlushBuffer can be written */
TEST(test_hw_fifo_level_cannot_be_written)
{
    hw_model_reset();
    spi_write(0x09, 0x11);
    spi_write(0x0A, 0x05);
    ASSERT_EQ(spi_read(0x0A), 1);
    spi_write(0x0A, 0x80);
    ASSERT_EQ(spi_read(0x0A), 0);
}

/* p.41: BufferOvfl is set when data is written "even though it is already
 * full". p.30: it "can only be cleared by setting the FIFOLevelReg register's
 * FlushBuffer bit". */
TEST(test_hw_fifo_overflow_sets_bufferovfl)
{
    hw_model_reset();
    for (int i = 0; i < 64; i++) spi_write(0x09, 0x00);
    ASSERT_EQ_HEX(spi_read(0x06) & 0x10, 0x00);
    spi_write(0x09, 0x00);
    ASSERT_EQ_HEX(spi_read(0x06) & 0x10, 0x10);
    ASSERT_EQ(spi_read(0x0A), 64);

    spi_write(0x0A, 0x80);
    ASSERT_EQ_HEX(spi_read(0x06) & 0x10, 0x00);
}

/* p.71: Mem "automatically terminates when finished and the Idle command
 * becomes active", and table 30 sets IdleIRq when a command terminates */
TEST(test_hw_mem_stores_25_bytes_and_goes_idle)
{
    hw_model_reset();
    spi_write(0x01, 0x00);
    spi_write(0x04, 0x7F);
    for (int i = 0; i < 25; i++) spi_write(0x09, (uint8_t)(i + 1));
    spi_write(0x01, 0x01);
    ASSERT_EQ(spi_read(0x0A), 0);
    ASSERT_EQ_HEX(spi_read(0x01) & 0x0F, 0x00);
    ASSERT_EQ_HEX(spi_read(0x04) & 0x10, 0x10);

    /* With an empty FIFO, Mem hands the 25 bytes back */
    spi_write(0x01, 0x01);
    ASSERT_EQ(spi_read(0x0A), 25);
    for (int i = 0; i < 25; i++) ASSERT_EQ(spi_read(0x09), i + 1);
}

/* p.40: "The microcontroller starting the Idle command does not set the
 * IdleIRq bit" */
TEST(test_hw_host_idle_sets_no_idleirq)
{
    hw_model_reset();
    spi_write(0x04, 0x7F);
    spi_write(0x01, 0x00);
    ASSERT_EQ_HEX(spi_read(0x04) & 0x10, 0x00);
}

/* p.40: on an unknown command "the CommandReg register Command[3:0] value
 * changes to the idle state and the IdleIRq bit is set" */
TEST(test_hw_unknown_command_goes_idle)
{
    hw_model_reset();
    spi_write(0x04, 0x7F);
    spi_write(0x01, 0x05);
    ASSERT_EQ_HEX(spi_read(0x01) & 0x0F, 0x00);
    ASSERT_EQ_HEX(spi_read(0x04) & 0x10, 0x10);
}

/* Steps 1 to 5 of section 16.1.1, p.82 */
static void self_test_steps(bool reset, bool clear_buffer, bool enable, uint8_t fifo)
{
    if (reset) spi_write(0x01, 0x0F);
    spi_write(0x0A, 0x80);
    if (clear_buffer) {
        for (int i = 0; i < 25; i++) spi_write(0x09, 0x00);
        spi_write(0x01, 0x01);
    }
    if (enable) spi_write(0x36, 0x09);
    spi_write(0x09, fifo);
    spi_write(0x01, 0x03);
    for (int i = 0; i < HW_SELF_TEST_POLLS; i++) (void)spi_read(0x0A);
}

/* The first and last four bytes printed for each version on p.82 */
TEST(test_hw_self_test_gives_the_printed_bytes)
{
    static const uint8_t head[2][4] = {{0x00, 0xC6, 0x37, 0xD5},
                                       {0x00, 0xEB, 0x66, 0xBA}};
    static const uint8_t tail[2][4] = {{0x25, 0x1D, 0x29, 0x79},
                                       {0x7D, 0x95, 0x3B, 0x2F}};
    for (int v = 0; v < 2; v++) {
        hw_model_reset();
        hw_regs[0x37] = (v == 0) ? 0x91 : 0x92;
        self_test_steps(true, true, true, 0x00);
        ASSERT_EQ(spi_read(0x0A), 64);
        for (int i = 0; i < 64; i++) {
            uint8_t b = spi_read(0x09);
            if (i < 4) ASSERT_EQ_HEX(b, head[v][i]);
            if (i >= 60) ASSERT_EQ_HEX(b, tail[v][i - 60]);
        }
    }
}

/* p.82 prints the result of this procedure only, so a step left out must not
 * produce it */
TEST(test_hw_self_test_needs_every_step)
{
    static const bool steps[4][3] = {{false, true, true}, {true, false, true},
                                     {true, true, false}, {true, true, true}};
    for (int s = 0; s < 4; s++) {
        hw_model_reset();
        self_test_steps(steps[s][0], steps[s][1], steps[s][2],
                        (s == 3) ? 0x5A : 0x00);
        bool signature = spi_read(0x0A) == 64;
        if (signature) {
            (void)spi_read(0x09);
            signature = spi_read(0x09) == 0xEB;
        }
        ASSERT_FALSE(signature);
    }
}

/* Section 16.1.1 used through the driver, for each version */
TEST(test_hw_driver_self_test_both_versions)
{
    hw_model_reset();
    ASSERT_EQ(rc522_init(&dev, &hw_spi), RC522_OK);
    ASSERT_EQ(rc522_self_test(&dev), RC522_OK);

    hw_model_reset();
    hw_regs[0x37] = RC522_VERSION_1_0;
    ASSERT_EQ(rc522_init(&dev, &hw_spi), RC522_OK);
    ASSERT_EQ(rc522_self_test(&dev), RC522_OK);
}

/* The configuration survives the reset inside the self-test, antenna included */
TEST(test_hw_card_answers_after_the_self_test)
{
    hw_model_reset();
    ASSERT_EQ(rc522_init(&dev, &hw_spi), RC522_OK);
    ASSERT_EQ(rc522_self_test(&dev), RC522_OK);
    hw_model_inject_atqa(0x04, 0x00);
    ASSERT_EQ(rc522_detect_card(&dev), RC522_OK);
}

/* p.46: TxLastBits "defines the number of bits of the last byte that will be
 * transmitted". REQA and its 7 bits come from ISO/IEC 14443-3, not this manual. */
TEST(test_hw_driver_sends_reqa_as_a_short_frame)
{
    hw_model_reset();
    ASSERT_EQ(rc522_init(&dev, &hw_spi), RC522_OK);
    hw_model_inject_atqa(0x04, 0x00);
    ASSERT_EQ(rc522_detect_card(&dev), RC522_OK);
    ASSERT_EQ(hw_tx_count, 1);
    ASSERT_EQ(hw_tx_len, 1);
    ASSERT_EQ_HEX(hw_tx_frame[0], 0x26);
    ASSERT_EQ(hw_tx_last_bits, 7);
}

/* A stale RxIRq must not pass for the answer: the driver's 7Fh clears it, and
 * it then waits for the card */
TEST(test_hw_stale_rxirq_is_not_the_answer)
{
    hw_model_reset();
    ASSERT_EQ(rc522_init(&dev, &hw_spi), RC522_OK);
    spi_write(0x04, 0xA0);
    hw_model_inject_atqa(0x04, 0x00);
    ASSERT_EQ(rc522_detect_card(&dev), RC522_OK);

    spi_write(0x04, 0xA0);
    ASSERT_EQ(rc522_detect_card(&dev), RC522_ERR_TIMEOUT);
}

/* Table 30: TxIRq is "set immediately after the last bit of the transmitted
 * data was sent out", and p.72 has the receive phase follow it */
TEST(test_hw_answer_comes_after_the_transmission)
{
    field_on_timer_auto();
    hw_model_inject_atqa(0x04, 0x00);
    start_transceive(0x07);
    spi_write(0x0D, 0x87);
    ASSERT_EQ_HEX(spi_read(0x04) & 0x61, 0x40);
    ASSERT_EQ(spi_read(0x0A), 0);
}

/* p.70: "Each command can be interrupted by the host writing a new command
 * code to the CommandReg register" */
TEST(test_hw_new_command_interrupts_the_exchange)
{
    field_on_timer_auto();
    hw_model_inject_atqa(0x04, 0x00);
    start_transceive(0x07);
    spi_write(0x0D, 0x87);
    spi_write(0x01, 0x00);
    for (int i = 0; i < HW_AIR_POLLS; i++) (void)spi_read(0x04);
    ASSERT_EQ_HEX(spi_read(0x04) & 0x21, 0x00);
}

/* p.82 step 1 is a soft reset: a chip configured after one is not at step 1 */
TEST(test_hw_self_test_needs_a_fresh_reset)
{
    hw_model_reset();
    spi_write(0x01, 0x0F);
    spi_write(0x2A, 0x8D);
    self_test_steps(false, true, true, 0x00);
    ASSERT_EQ(spi_read(0x0A), 64);
    (void)spi_read(0x09);
    ASSERT_TRUE(spi_read(0x09) != 0xEB);
}

/* p.82: the bytes are there "When the self test has completed" */
TEST(test_hw_self_test_takes_time)
{
    hw_model_reset();
    spi_write(0x01, 0x0F);
    spi_write(0x0A, 0x80);
    for (int i = 0; i < 25; i++) spi_write(0x09, 0x00);
    spi_write(0x01, 0x01);
    spi_write(0x36, 0x09);
    spi_write(0x09, 0x00);
    spi_write(0x01, 0x03);
    ASSERT_EQ(spi_read(0x0A), 1);
}

/* p.41: receiver start-up clears the errors of the last frame, so a good answer
 * after a bad one succeeds */
TEST(test_hw_errors_clear_on_the_next_reception)
{
    hw_model_reset();
    ASSERT_EQ(rc522_init(&dev, &hw_spi), RC522_OK);
    hw_model_inject_atqa(0x04, 0x00);
    hw_rx_errors = 0x02;
    ASSERT_EQ(rc522_detect_card(&dev), RC522_ERR_SPI);
    ASSERT_EQ_HEX(spi_read(0x04) & 0x02, 0x02);  /* ErrIRq, table 30 */

    hw_model_inject_atqa(0x04, 0x00);
    ASSERT_EQ(rc522_detect_card(&dev), RC522_OK);
}

/* p.11: "Bits 6 to 1 define the address and the LSB is set to logic 0." */
TEST(test_hw_driver_keeps_address_bit0_clear)
{
    hw_model_reset();
    ASSERT_EQ(rc522_init(&dev, &hw_spi), RC522_OK);
    ASSERT_EQ(rc522_self_test(&dev), RC522_OK);
    hw_model_inject_uid(0xCA, 0xFE, 0xBA, 0xBE);
    rc522_uid_t uid;
    ASSERT_EQ(rc522_read_card_uid(&dev, &uid), RC522_OK);
    ASSERT_EQ(hw_addr_errors, 0);
}

TEST_MAIN("rc522_hw_model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_init_v1);
    RUN(test_hw_detect_card_with_atqa);
    RUN(test_hw_read_uid);
    RUN(test_hw_power_on_values_follow_the_manual);
    RUN(test_hw_soft_reset_restores_every_register);
    RUN(test_hw_soft_reset_keeps_the_version);
    RUN(test_hw_card_answers_only_after_start_send);
    RUN(test_hw_start_send_needs_transceive);
    RUN(test_hw_no_field_no_answer);
    RUN(test_hw_one_tx_driver_is_a_field);
    RUN(test_hw_rcvoff_blocks_the_answer);
    RUN(test_hw_timer_runs_out_after_the_transmission);
    RUN(test_hw_timer_needs_tauto);
    RUN(test_hw_answer_stops_the_timer);
    RUN(test_hw_comirq_set1_marks_bits);
    RUN(test_hw_read_only_registers_ignore_writes);
    RUN(test_hw_start_send_reads_zero);
    RUN(test_hw_fifo_level_cannot_be_written);
    RUN(test_hw_fifo_overflow_sets_bufferovfl);
    RUN(test_hw_mem_stores_25_bytes_and_goes_idle);
    RUN(test_hw_host_idle_sets_no_idleirq);
    RUN(test_hw_unknown_command_goes_idle);
    RUN(test_hw_self_test_gives_the_printed_bytes);
    RUN(test_hw_self_test_needs_every_step);
    RUN(test_hw_driver_self_test_both_versions);
    RUN(test_hw_card_answers_after_the_self_test);
    RUN(test_hw_driver_sends_reqa_as_a_short_frame);
    RUN(test_hw_stale_rxirq_is_not_the_answer);
    RUN(test_hw_answer_comes_after_the_transmission);
    RUN(test_hw_new_command_interrupts_the_exchange);
    RUN(test_hw_self_test_needs_a_fresh_reset);
    RUN(test_hw_self_test_takes_time);
    RUN(test_hw_errors_clear_on_the_next_reception);
    RUN(test_hw_driver_keeps_address_bit0_clear);
TEST_MAIN_END()
