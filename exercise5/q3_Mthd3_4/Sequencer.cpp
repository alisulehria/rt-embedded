 /*
 * This is a C++ version of the canonical pthread service example. It intends
 * to abstract the service management functionality and sequencing for ease
 * of use. Much of the code is left to be implemented by the student.
 *
 * Build with g++ --std=c++23 -Wall -Werror -pedantic
 * Steve Rizor 3/16/2025
 * Edited by : Sriya Garde
 */
#include <cstdint>
#include <cstdio>
#include "Sequencer.hpp"
#include <linux/gpio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>

#define DEV_NAME "/dev/gpiochip0"
#define DEV_MEM "/dev/gpiomem"
#define GPIO_WRITE 17 // this is "offset"

static int state = 0;

typedef enum
{
    APP_OPT_GPIO_LIST,
    APP_OPT_GPIO_READ,
    APP_OPT_GPIO_WRITE,
    APP_OPT_GPIO_POLL,
    APP_OPT_UNKNOWN
} app_mode_t;
typedef struct
{
    char *dev;
    int offset;
    uint8_t val;
    app_mode_t mode;
} app_opt_t;

static void gpio_write(const char *dev_name, int offset, uint8_t value){
    //part 3b implementation (adapted from lxsang.me)
 
    printf("Writing value %d to GPIO at offset %d on chip %s \n", value, offset, dev_name); //offset is pin number
    int fd, ret;
    struct gpiohandle_request rq;
    struct gpiohandle_data data;
    // read file, throw error if unable
    fd = open(dev_name, O_RDONLY);
    if(fd < 0)
    {
        printf("Unable to open %s: %s\n", DEV_NAME, strerror(errno));
        return;
    }
    // setting gpio request to be an output, other params
    rq.lineoffsets[0] = offset;
    rq.flags = GPIOHANDLE_REQUEST_OUTPUT;
    rq.lines = 1;
    //get line handle of 
    ret = ioctl(fd, GPIO_GET_LINEHANDLE_IOCTL, &rq);
    if(ret  == -1){
        printf("Cannot get line handle: %s \n", strerror(errno));
    }
    data.values[0] = value;
    ret = ioctl(rq.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data);
    if (ret == -1){
        printf("Unable to set pin state: %s\n", strerror(errno));
        close(fd);
        return;
    }
    close(rq.fd);
}


static void mmap_gpio_write(const char *dev_mem, int offset, uint8_t value){
    //part 3d implementation (mmap call adapted in part from Dr. Lawlor's lecture at https://www.cs.uaf.edu/2016/fall/cs301/lecture/11_09_raspberry_pi.html)
    int fd;
    fd = open(dev_mem, O_RDWR);
    if (fd < 0){
        printf("Unable to open %s: %s\n", DEV_MEM, strerror(errno));
        return;
    }
    unsigned int *gpio = (unsigned int *)mmap(0, 4096, PROT_READ+PROT_WRITE, MAP_SHARED, fd, 0);
    if (gpio == MAP_FAILED){
        printf("mmap failed: %s\n", strerror(errno));
        close(fd);
        return;
    }
    printf("mmap success: %s at address %d\n", dev_mem, gpio);
    close(fd);
    // write the value using either gpio[7] (hi) or gpio[10] (low)
    gpio[index_offset] = 1 << offset;

    munmap(gpio, 4096);
    return;
}

void service() {

    //std::puts("this is service 2 implemented as a function\n");
    // system("sudo pinctrl get 17");
    // part 3a implementation:
    
    if(state == 0)
    {
		// system("sudo pinctrl set 17 dh"); //part a 
        //gpio_write(DEV_NAME, GPIO_WRITE, 1); //part c
        mmap_gpio_write(DEV_MEM, GPIO_WRITE, 1);
		state = 1;
	}
	else if(state == 1)
	{
		// system("sudo pinctrl set 17 dl"); // part a
        //gpio_write(DEV_NAME, GPIO_WRITE, 0); // part c
        mmap_gpio_write(DEV_MEM, GPIO_WRITE, 0);
		state = 0;
	}
    
}

int main() {
    Sequencer sequencer{};
    
    /*sequencer.addService([]() {
        std::puts("this is service 1 implemented in a lambda expression\n");
    }, 1, 99, 500); // 500 ms period
	*/
	
	system("sudo pinctrl set 17 op dl");
    sequencer.addService(service, 1, 99, 100); // 100 ms period

    sequencer.startServices();
    
    // Wait for Enter key to stop services
    sequencer.waitForStopSignal();
    
    std::cout << "All services stopped. Exiting...\n";
    return 0;
}
