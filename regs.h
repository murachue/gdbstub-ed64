#include "3264.h"

struct {
	uint32_t start;
	uint32_t end;
	uint32_t current;
	uint32_t status;
	uint32_t clock; // clock counter
	uint32_t bufbusy; // buffer busy counter
	uint32_t pipebusy; // pipe busy counter
	uint32_t tmem; // tmem load counter
} static volatile * const DP = (void*)P32(0xA4100000);
enum DP_STATUS_READ {
	DP_STATUS_XBUS_DMEM_DMA = 1 << 0,
	DP_STATUS_FREEZE        = 1 << 1,
	DP_STATUS_FLUSH         = 1 << 2,
	DP_STATUS_START_GCLK    = 1 << 3,
	DP_STATUS_TMEM_BUSY     = 1 << 4,
	DP_STATUS_PIPE_BUSY     = 1 << 5,
	DP_STATUS_CMD_BUSY      = 1 << 6,
	DP_STATUS_CBUF_READY    = 1 << 7,
	DP_STATUS_DMA_BUSY      = 1 << 8,
	DP_STATUS_END_VALID     = 1 << 9,
	DP_STATUS_START_VALID   = 1 << 10,
};
enum DP_STATUS_WRITE {
	DP_STATUS_CLEAR_XBUS_DMEM_DMA = 1 << 0, // rdram->dp
	DP_STATUS_SET_XBUS_DMEM_DMA   = 1 << 1, // dmem->dp
	DP_STATUS_CLEAR_FREEZE        = 1 << 2,
	DP_STATUS_SET_FREEZE          = 1 << 3,
	DP_STATUS_CLEAR_FLUSH         = 1 << 4,
	DP_STATUS_SET_FLUSH           = 1 << 5,
	DP_STATUS_CLEAR_TMEM_CTR      = 1 << 6,
	DP_STATUS_CLEAR_PIPE_CTR      = 1 << 7,
	DP_STATUS_CLEAR_CMD_CTR       = 1 << 8,
	DP_STATUS_CLEAR_CLOCK_CTR     = 1 << 9,
};

struct {
	uint32_t mode;
	uint32_t version;
	uint32_t intr;
	uint32_t intrmask;
} static volatile * const MI = (void*)P32(0xA4300000);
enum {
	MI_INTRMASK_SET_VI = 1 << 7,
};

struct {
	uint32_t control;
	uint32_t dramaddr;
	uint32_t width;
	uint32_t intr;
	uint32_t current;
	uint32_t timing;
	uint32_t vsync;
	uint32_t hsync;
	uint32_t hsync_leap;
	uint32_t hrange;
	uint32_t vrange;
	uint32_t vburstrange;
	uint32_t xscale;
	uint32_t yscale;
} static volatile * const VI = (void*)P32(0xA4400000);

struct {
	uint32_t dramaddr;
	uint32_t cartaddr;
	uint32_t dram2cart; // read (to SI)
	uint32_t cart2dram; // write (from SI)
	uint32_t status;
	uint32_t dom1lat; // latency
	uint32_t dom1pwd; // pulse width
	uint32_t dom1pgs; // page size
	uint32_t dom1rls; // release
	uint32_t dom2lat; // latency
	uint32_t dom2pwd; // pulse width
	uint32_t dom2pgs; // page size
	uint32_t dom2rls; // release
} static volatile * const __attribute((unused)) PI = (void*)P32(0xA4600000);
enum PI_STATUS_READ {
	PI_STATUS_DMABUSY = 1 << 0,
	PI_STATUS_IOBUSY  = 1 << 1,
	PI_STATUS_ERROR   = 1 << 2,
};
enum PI_STATUS_WRITE {
	PI_STATUS_RESET     = 1 << 0,
	PI_STATUS_CLEARINTR = 1 << 1,
};

struct {
	uint32_t dramaddr;
	uint32_t pifram2dram;
	uint32_t _reserved08;
	uint32_t _reserved0c;
	uint32_t dram2pifram;
	uint32_t _reserved14;
	uint32_t status;
} static volatile * const SI = (void*)P32(0xA4800000);
enum SI_STATUS_READ {
	SI_STATUS_DMABUSY  = 1 <<  0,
	SI_STATUS_IOBUSY   = 1 <<  1,
	SI_STATUS_DMAERROR = 1 <<  3,
	SI_STATUS_INTR     = 1 << 12,
};

struct {
	uint32_t cfg;
	uint32_t status;
	uint32_t dmalen;
	uint32_t dmaaddr;
	uint32_t msg;
	uint32_t dmacfg;
	uint32_t spi;
	uint32_t spicfg;
	uint32_t key;
	uint32_t savcfg;
	uint32_t sec;
	uint32_t ver;
} static volatile * const ED64 = (void*)P32(0xA8040000);
enum ED64_CFG {
	ED64_CFG_SDRAM_ON     = 1 << 0,
	ED64_CFG_SWAP         = 1 << 1,
	ED64_CFG_WR_MOD       = 1 << 2,
	ED64_CFG_WR_ADDR_MASK = 1 << 3,
};
enum ED64_STATUS {
	ED64_STATUS_DMABUSY    = 1 << 0,
	ED64_STATUS_DMATIMEOUT = 1 << 1,
	ED64_STATUS_TXE        = 1 << 2,
	ED64_STATUS_RXF        = 1 << 3,
	ED64_STATUS_SPI        = 1 << 4,
};
enum ED64_DMACFG {
	ED64_DMACFG_SD_TO_RAM   = 1,
	ED64_DMACFG_RAM_TO_SD   = 2,
	ED64_DMACFG_FIFO_TO_RAM = 3,
	ED64_DMACFG_RAM_TO_FIFO = 4,
};
