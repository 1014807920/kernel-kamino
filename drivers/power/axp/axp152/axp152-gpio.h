#ifndef AXP152_GPIO_H

#define AXP152_GPIO_H
#include "axp152.h"

#define AXP_GPIO_IRQ_EDGE_RISING   (0x1<<7)
#define AXP_GPIO_IRQ_EDGE_FALLING  (0x1<<6)
#define AXP_GPIO_INPUT_TRIG_MASK   (0x7<<0)
#define AXP_GPIO_EDGE_TRIG_MASK    (AXP_GPIO_IRQ_EDGE_RISING | \
				AXP_GPIO_IRQ_EDGE_FALLING)

#define AXP_GPIO0_CFG        (AXP152_GPIO0_CTL)       /* 0x90 */
#define AXP_GPIO1_CFG        (AXP152_GPIO1_CTL)       /* 0x91 */
#define AXP_GPIO2_CFG        (AXP152_GPIO2_CTL)       /* 0x92 */
#define AXP_GPIO3_CFG        (AXP152_GPIO3_CTL)       /* 0x93 */
#define AXP_GPIO_STATE       (AXP152_GPIO0123_SIGNAL) /* 0x97 */
#define AXP_GPIO0123_INTEN   (AXP152_INTEN3)          /* 0x42 */
#define AXP_GPIO0123_INTSTA  (AXP152_INTSTS3)         /* 0x4A */

#endif
