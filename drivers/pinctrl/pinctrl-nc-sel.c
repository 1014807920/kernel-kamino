#if defined(CONFIG_ARCH_LEO)
static const struct padmux_sel leo_a7[] = {
	/*    bit0	| bit1	| bit2			function0  | function1 | function2 | function3 | function4 | function5 */
	{ { 0,	PMUX_INV, PMUX_INV } }, // BTDAT00    | PD2PORT00
	{ { 1,	PMUX_INV, PMUX_INV } }, // BTDAT01    | PD2PORT01
	{ { 2,	PMUX_INV, PMUX_INV } }, // BTDAT02    | PD2PORT02
	{ { 3,	PMUX_INV, PMUX_INV } }, // BTDAT03    | PD2PORT03
	{ { 4,	PMUX_INV, PMUX_INV } }, // BTCLKIN    | PD2PORT04
	{ { 5,	PMUX_INV, PMUX_INV } }, // BTRESET    | PD2PORT05
	{ { 6,	PMUX_INV, PMUX_INV } }, // BTCLKVSYNC | PD2PORT06
	{ { 7,	PMUX_INV, PMUX_INV } }, // BTCLKHREF  | PD2PORT07
	{ { 8,	PMUX_INV, PMUX_INV } }, // BTCLKOUT   | PD2PORT08
	{ { 9,	      64, PMUX_INV } }, // BTDAT04    | SDA2      | SPI1SCK   | PD2PORT09
	{ {10,	      65, PMUX_INV } }, // BTDAT05    | SCL2      | SPI1MOSI  | PD2PORT10
	{ {11,	      66, PMUX_INV } }, // BTDAT06    | UART2RX   | SPI1CSn   | PD2PORT11
	{ {12,	      67,       96 } }, // BTDAT07    | UART2TX   | SPI1MISO  | NUARTTX   | AUARTTX   | PD2PORT12
	{ {13,	PMUX_INV, PMUX_INV } }, // BTDAT08    | PD2PORT13
	{ {14,	PMUX_INV, PMUX_INV } }, // BTDAT09    | PD2PORT14
	{ {15,	PMUX_INV, PMUX_INV } }, // BTDAT10    | PD2PORT15
	{ {16,	PMUX_INV, PMUX_INV } }, // BTDAT11    | PD2PORT16
	{ {17,	PMUX_INV, PMUX_INV } }, // BTDAT12    | PD2PORT17
	{ {18,	PMUX_INV, PMUX_INV } }, // BTDAT13    | PD2PORT18
	{ {19,	PMUX_INV, PMUX_INV } }, // BTDAT14    | PD2PORT19
	{ {20,	PMUX_INV, PMUX_INV } }, // BTDAT15    | PD2PORT20
	{ {21,	      68, PMUX_INV } }, // UART3RX    | SD1CDn    | SDA3      | PD2PORT21
	{ {22,	      69, PMUX_INV } }, // UART3TX    | SD1DAT1   | SCL3      | PD2PORT22
	{ {23,	      70, PMUX_INV } }, // DBGTDI     | SD1DAT0   | SPI2SCK   | PD2PORT23
	{ {24,	      71, PMUX_INV } }, // DBGTDO     | SD1CLK    | SPI2MOSI  | PD2PORT24
	{ {25,	      72, PMUX_INV } }, // DBGTMS     | SD1CMD    | SPI2CSn   | PD2PORT25
	{ {26,	      73, PMUX_INV } }, // DBGTCK     | SD1DAT3   | SPI2MISO  | PD2PORT26
	{ {27,	      74, PMUX_INV } }, // DBGTRST    | SD1DAT2   | PD2PORT27
	{ {28,	      75, PMUX_INV } }, // UART2RX    | SDA2      | PD2PORT28
	{ {29,	      76, PMUX_INV } }, // UART2TX    | SCL2      | PD2PORT29
	{ {30,	      77, PMUX_INV } }, // SPI1SCK    | NDBGTDI   | ADBGTDI   | PD2PORT30
	{ {31,	      78, PMUX_INV } }, // SPI1MOSI   | NDBGTDO   | ADBGTDO   | PD2PORT31
	{ {32,	      79, PMUX_INV } }, // SPI1CSn    | NDBGTMS   | ADBGTMS   | PD2PORT32
	{ {33,	      80, PMUX_INV } }, // SPI1MISO   | NDBGTCK   | ADBGTCK   | PD2PORT33
	{ {34,	      81, PMUX_INV } }, // SPI2SCK    | NDBGTRST  | ADBGTRST  | PD2PORT34
	{ {35,	PMUX_INV, PMUX_INV } }, // SPI2MOSI   | PD2PORT35
	{ {36,	PMUX_INV, PMUX_INV } }, // SPI2CSn    | PD2PORT36
	{ {37,	PMUX_INV, PMUX_INV } }, // SPI2MISO   | PD2PORT37
	{ {38,	PMUX_INV, PMUX_INV } }, // SD0CDn     | PD2PORT38
	{ {39,	PMUX_INV, PMUX_INV } }, // SD0DAT1    | PD2PORT39
	{ {40,	PMUX_INV, PMUX_INV } }, // SD0DAT0    | PD2PORT40
	{ {41,	PMUX_INV, PMUX_INV } }, // SD0CLK     | PD2PORT41
	{ {42,	PMUX_INV, PMUX_INV } }, // SD0CMD     | PD2PORT42
	{ {43,	PMUX_INV, PMUX_INV } }, // SD0DAT3    | PD2PORT43
	{ {44,	PMUX_INV, PMUX_INV } }, // SD0DAT2    | PD2PORT44
	{ {45,	PMUX_INV, PMUX_INV } }, // SD1CDn     | PD2PORT45
	{ {46,	PMUX_INV, PMUX_INV } }, // SD1DAT1    | PD2PORT46
	{ {47,	PMUX_INV, PMUX_INV } }, // SD1DAT0    | PD2PORT47
	{ {48,	PMUX_INV, PMUX_INV } }, // SD1CLK     | PD2PORT48
	{ {49,	PMUX_INV, PMUX_INV } }, // SD1CMD     | PD2PORT49
	{ {50,	PMUX_INV, PMUX_INV } }, // SD1DAT3    | PD2PORT50
	{ {51,	PMUX_INV, PMUX_INV } }, // SD1DAT2    | PD2PORT51
};

static const struct padmux_sel leo_csky[] = {
	/*    bit0 | bit1   | bit2   func    function0  | function1 | function2   | function3  | function4 */
	{ { PMUX_INV, PMUX_INV, PMUX_INV } }, // only gpio mode ?
	{ { 1, PMUX_INV, PMUX_INV } },  // POWERDOWN  | PD1PORT01
	{ { 2, PMUX_INV, PMUX_INV } },  // UART0RX    | PD1PORT02
	{ { 4, PMUX_INV, PMUX_INV } },  // UART0TX    | PD1PORT03
	{ { 5, PMUX_INV, PMUX_INV } },  // OTPAVDDEN  | PD1PORT04
	{ { 3,       64, PMUX_INV } },  // SDBGTDI    | DDBGTDI   | SNDBGTDI    | PD1PORT05
	{ { 6,       65, PMUX_INV } },  // SDBGTDO    | DDBGTDO   | SNDBGTDO    | PD1PORT06
	{ { 7,       66,       96 } },  // SDBGTMS    | DDBGTMS   | SNDBGTMS    | PCM1INBCLK | PD1PORT07
	{ { 8,       67,       97 } },  // SDBGTCK    | DDBGTCK   | SNDBGTCK    | PCM1INLRCK | PD1PORT08
	{ { 9,       68,       98 } },  // SDBGTRST   | DDBGTRST  | SNBGTRST    | PCM1INDAT0 | PD1PORT09
	{ {PMUX_INV, PMUX_INV, PMUX_INV } },  //
	{ {10, PMUX_INV, PMUX_INV } },  // PCM1INBCLK | PD1PORT11
	{ {11, PMUX_INV, PMUX_INV } },  // PCM1INLRCK | PD1PORT12
	{ {12, PMUX_INV, PMUX_INV } },  // PCM1INDAT0 | PD1PORT13
	{ {13,       69, PMUX_INV } },  // PCMOUTMCLK | DUARTTX   | SNUARTTX    | PD1PORT14
	{ {14,       70, PMUX_INV } },  // PCMOUTDAT0 | SPDIF     | PD1PORT15
	{ {15, PMUX_INV, PMUX_INV } },  // PCMOUTLRCK | PD1PORT16
	{ {16, PMUX_INV, PMUX_INV } },  // PCMOUTBCLK | PD1PORT17
	{ {17, PMUX_INV, PMUX_INV } },  // UART1RX    | PD1PORT18
	{ {18, PMUX_INV, PMUX_INV } },  // UART1TX    | PD1PORT19
	{ {19,       71, PMUX_INV } },  // DDBGTDI    | SNDBGTDI  | PD1PORT20
	{ {20,       72, PMUX_INV } },  // DDBGTDO    | SNDBGTDO  | PD1PORT21
	{ {21,       73, PMUX_INV } },  // DDBGTMS    | SNDBGTMS  | PD1PORT22
	{ {22,       74, PMUX_INV } },  // DDBGTCK    | SNDBGTCK  | PD1PORT23
	{ {23,       75, PMUX_INV } },  // DDBGTRST   | SNDBGTRST | PD1PORT24
	{ {24,       76, PMUX_INV } },  // DUARTTX    | SNUARTTX  | PD1PORT25
	{ {25, PMUX_INV, PMUX_INV } },  // SDA0       | PD1PORT26
	{ {26, PMUX_INV, PMUX_INV } },  // SCL0       | PD1PORT27
	{ {27, PMUX_INV, PMUX_INV } },  // SDA1       | PD1PORT28
	{ {28, PMUX_INV, PMUX_INV } },  // SCL1       | PD1PORT29
	{ {29,       77, PMUX_INV } },  // PCM0INDAT1 | PDMDAT3   | PD1PORT30
	{ {30,       78, PMUX_INV } },  // PCM0INDAT0 | PDMDAT2   | PD1PORT31
	{ {31,       79, PMUX_INV } },  // PCM0INMCLK | PDMDAT1   | PD1PORT32
	{ {32,       80, PMUX_INV } },  // PCM0INLRCK | PDMDAT0   | PCM0OUTLRCK | PD1PORT33
	{ {33,       81, PMUX_INV } },  // PCM0INBCLK | PDMCLK    | PCM0OUTBCLK | PD1PORT34
	{ {34, PMUX_INV, PMUX_INV } },  // IR         | PD1PORT35
};

struct padmux_sel_table table_leo_a7 = {
	.nr_padmux = ARRAY_SIZE(leo_a7),
	.sel_table = leo_a7,
};

struct padmux_sel_table table_leo_csky = {
	.nr_padmux = ARRAY_SIZE(leo_csky),
	.sel_table = leo_csky,
};
#endif

#if defined(CONFIG_SIRIUS)
static const struct padmux_sel pinmux_sel_sirius_generic[] = {
	/*    bit0 | bit1   | bit2   func    function0  | function1 | function2   | function3  | function4 */
	{ {32,    PMUX_INV,    PMUX_INV } },           //NC          |PORT00/IR
	{ {33,    PMUX_INV,    PMUX_INV } },           //NC          |DBGTDI/PORT01
	{ {34,    123,         PMUX_INV } },           //NC          |DBGDTO/PORT02/CEC
	{ {35,    96,          PMUX_INV } },           //NC          |DBGTMS/PORT03/NFRDY0
	{ {36,    96,          PMUX_INV } },           //NC          |DBGTCK/PORT04/NFOE
	{ {37,    96,          PMUX_INV } },           //NC          |DBGTRST/PORT05/NFCLE
	{ {38,    97,          PMUX_INV } },           //NC          |PORT06/SPISCK/NFALE/SPISCK_DW
	{ {39,    98,          PMUX_INV } },           //NC          |PORT07/SPIMOSI/NFWE/SPIMOSI_DW
	{ {87,    99,          PMUX_INV } },           //NC          |SPICSn/NFDATA7/SPICSn_DW
	{ {40,    100,         PMUX_INV } },           //NC          |PORT08/SPIMISO/NFDATA6/SPIMISO_DW
	{ {41,    122,         PMUX_INV } },           //NC          |PORT09/TSI1VALID/TSOVALID
	{ {42,    101,         PMUX_INV } },           //NC          |PORT10/SC1CLK/TSI1DATA0/TSODATA0
	{ {43,    102,         PMUX_INV } },           //NC          |PORT11/SC1RST/TSI1DATA1/TSODATA1
	{ {44,    103,         PMUX_INV } },           //NC          |PORT12/SC1PWR/TSI1DATA2/TSODATA2
	{ {45,    104,         PMUX_INV } },           //NC          |PORT13/SC1CD/TSI1DATA3/TSODATA3
	{ {46,    105,         PMUX_INV } },           //NC          |PORT14/SC1DATA/TSI1DATA4/TSODATA4
	{ {47,    106,         133      } },           //NC          |PORT15/UART2TX/TSI1DATA5/TSODATA5/NFDATA5
	{ {48,    107,         134      } },           //NC          |PORT16/UART2RX/TSI1DATA6/TSODATA6/NFDAT4
	{ {49,    108,         135      } },           //NC          |PORT17/SDA2/TSI1DATA7/TSODATA7/NFDAT3
	{ {50,    109,         136      } },           //NC          |PORT18/SCL2/TSI1CLK/TSOCLK/NFDAT2
	{ {51,    110,         PMUX_INV } },           //NC          |PORT19/TSI1SYNC/TSOSYNC
	{ {52,    120,         PMUX_INV } },           //NC          |PORT20/TSI2VALID/TSOVALID
	{ {53,    120,         PMUX_INV } },           //NC          |PORT21/TSI2DATA0/TSODATA0
	{ {54,    120,         PMUX_INV } },           //NC          |PORT22/TSI2DATA1/TSODATA1
	{ {55,    120,         PMUX_INV } },           //NC          |PORT23/TSI2DATA2/TSODATA2
	{ {56,    120,         PMUX_INV } },           //NC          |PORT24/TSI2DATA3/TSODATA3
	{ {57,    120,         PMUX_INV } },           //NC          |PORT25/TSI2DATA4/TSODATA4
	{ {58,    120,         PMUX_INV } },           //NC          |PORT26/TSI2DATA5/TSODATA5
	{ {59,    120,         PMUX_INV } },           //NC          |PORT27/TSI2DATA6/TSODATA6
	{ {60,    120,         PMUX_INV } },           //NC          |PORT28/TSI2DATA7/TSODATA7
	{ {61,    120,         PMUX_INV } },           //NC          |PORT29/TSI2SCLK/TSOCLK
	{ {62,    120,         PMUX_INV } },           //NC          |PORT30/TSI2SYNC/TSOSYNC
	{ {63,    PMUX_INV,    PMUX_INV } },           //NC          |PORT31/SDA2
	{ {64,    PMUX_INV,    PMUX_INV } },           //NC          |PORT32/SCL2
	{ {65,    111,         PMUX_INV } },           //NC          |PORT33/UART2TX/AUARTTX
	{ {66,    PMUX_INV,    PMUX_INV } },           //NC          |PORT34/UART2RX
	{ {67,    113,         132      } },           //NC          |PORT35/HVSEL/SDA3/NFDAT1/DiSEqCi
	{ {68,    114,         PMUX_INV } },           //NC          |PORT36/DiSEqCo/SCL3/NFDAT0
	{ {69,    115,         PMUX_INV } },           //NC          |PORT37/AGC1/AGC2
	{ {70,    116,         PMUX_INV } },           //NC          |UART1TX/PORT38/AUARTTX
	{ {71,    PMUX_INV,    PMUX_INV } },           //NC          |UART1RX/PORT39
	{ {72,    119,         PMUX_INV } },           //NC          |PORT40/SDA1/SDA_T
	{ {73,    119,         PMUX_INV } },           //NC          |PORT41/SCL1/SCL_T
	{ {80,    PMUX_INV,    PMUX_INV } },           //NC          |PORT48/CEC
	{ {74,    117,         PMUX_INV } },           //NC          |PORT42/DDCSDA/SDA2
	{ {75,    118,         PMUX_INV } },           //NC          |PORT43/DDCSCL/SCL2
	{ {76,    PMUX_INV,    PMUX_INV } },           //NC          |PORT44/SPDIF
	{ {77,    PMUX_INV,    PMUX_INV } },           //NC          |PORT45/SPD_LED
	{ {78,    PMUX_INV,    PMUX_INV } },           //NC          |PORT46/LINK_LED
	{ {79,    PMUX_INV,    PMUX_INV } },           //NC          |PORT47/ACT_LED
	{ {85,    PMUX_INV,    PMUX_INV } },           //NC          |PORT64(GBPORT00)/SC1CLK
	{ {86,    PMUX_INV,    PMUX_INV } },           //NC          |PORT70(GBPORT06)/NFRDY0
	{ {86,    PMUX_INV,    PMUX_INV } },           //NC          |PORT71(GBPORT07)/NFOE
	{ {86,    PMUX_INV,    PMUX_INV } },           //NC          |PORT72(GBPORT08)/NFCLE
	{ {86,    PMUX_INV,    PMUX_INV } },           //NC          |PORT73(GBPORT09)/NFALE
	{ {86,    PMUX_INV,    PMUX_INV } },           //NC          |PORT74(GBPORT10)/NFWE
	{ {88,    PMUX_INV,    PMUX_INV } },           //NC          |PORT75(GBPORT11)/NFDATA7
	{ {88,    PMUX_INV,    PMUX_INV } },           //NC          |PORT76(GBPORT12)/NFDATA6
	{ {88,    PMUX_INV,    PMUX_INV } },           //NC          |PORT77(GBPORT13)/NFDATA5
	{ {88,    PMUX_INV,    PMUX_INV } },           //NC          |PORT78(GBPORT14)/NFDATA4
	{ {88,    PMUX_INV,    PMUX_INV } },           //NC          |PORT79(GBPORT15)/NFDATA3
	{ {88,    PMUX_INV,    PMUX_INV } },           //NC          |PORT90(GBPORT16)/NFDATA2
	{ {88,    PMUX_INV,    PMUX_INV } },           //NC          |PORT81(GBPORT17)/NFDATA1
	{ {88,    PMUX_INV,    PMUX_INV } },           //NC          |PORT82(GBPORT18)/NFDATA0
	{ {81,    PMUX_INV,    PMUX_INV } },           //NC          |PORT83(GBPORT19)/TSI3VALID
	{ {82,    121,         PMUX_INV } },           //NC          |PORT84(GBPORT20)/TSI3DATA0/TSISVALID
	{ {82,    121,         PMUX_INV } },           //NC          |PORT85(GBPORT21)/TSI3DATA1/TSISDATA
	{ {82,    121,         PMUX_INV } },           //NC          |PORT86(GBPORT22)/TSI3DATA2/TSISCLK
	{ {82,    121,         PMUX_INV } },           //NC          |PORT87(GBPORT23)/TSI3DATA3/TSISSYNC
	{ {83,    PMUX_INV,    PMUX_INV } },           //NC          |PORT88(GBPORT24)/TSI3DATA4
	{ {83,    PMUX_INV,    PMUX_INV } },           //NC          |PORT89(GBPORT25)/TSI3DATA5
	{ {84,    PMUX_INV,    PMUX_INV } },           //NC          |PORT90(GBPORT26)/TSI3DATA6
	{ {84,    PMUX_INV,    PMUX_INV } },           //NC          |PORT91(GBPORT27)/TSI3DATA7
	{ {84,    PMUX_INV,    PMUX_INV } },           //NC          |PORT92(GBPORT28)/TSI3CLK
	{ {84,    PMUX_INV,    PMUX_INV } },           //NC          |PORT93(GBPORT29)/TSI3SYNC
};

struct padmux_sel_table table_sirius_generic = {
	.nr_padmux = ARRAY_SIZE(pinmux_sel_sirius_generic),
	.sel_table = pinmux_sel_sirius_generic,
};
#endif

static const struct of_device_id nc_of_match[] = {
#if defined(CONFIG_ARCH_LEO)
	{ .compatible =  "NationalChip-pinctrl-leo-a7",
		.data =  &table_leo_a7},
	{ .compatible =  "NationalChip-pinctrl-leo-csky",
		.data =  &table_leo_csky},
#endif
#if defined(CONFIG_SIRIUS)
	{ .compatible =  "NationalChip-pinctrl-sirius-generic",
		.data =  &table_sirius_generic},
#endif
	{ },
};
MODULE_DEVICE_TABLE(of, nc_of_match);

