#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

const int STEP_PER_MM_X = 1;
const int STEP_PER_MM_Y = 1;
const int STEP_PER_MM_Z = 1;
//ports:
#define FLAG_X (1 << 0)
#define FLAG_Y (1 << 1)
#define FLAG_Z (1 << 2)
#define FLAG_F (1 << 3)

typedef struct{
    uint8_t command;
    uint8_t flags;
    int32_t x, y, z;
    uint16_t feedrate;
}__attribute__((packed)) BinaryFlags;

typedef enum{
    //G17 = 17, //select XY plane
    G90 = 90, //absolute positioning
    G28 = 28, //autohome, base position
    G00 = 0, //rapid movement, move to position at max speed
    G01 = 1,  //Move to position with feed rate
    G21 = 21, //set to mm
    M30 = 30  //end of program
}GcodeType;

typedef struct{
    GcodeType command;
    int x, y, z;
    int feedrate;
    int has_x, has_y, has_z, has_f;
}GcodeCommand;

GcodeType parser(const char* line, GcodeCommand* cmd)
{
    memset(cmd, 0, sizeof(GcodeCommand)); //zeros out everything
    char buffer[128];
    strncpy(buffer, line, sizeof(buffer) -1);
    buffer[sizeof(buffer) -1] = '\0';

    char* token = strtok(buffer, " \t\n");
    while(token != NULL)
    {
        int gnumber = atoi(&token[1]);
        switch(token[0])
        {
            case 'G':
                switch(gnumber)
                {
                    case 28: cmd->command = G28; break;
                    case 90: cmd->command = G90; break;
                    case 21: cmd->command = G21; break;
                    case 1: cmd->command = G01; break;
                    case 0: cmd->command = G00; break;
                    default: cmd->command = -1; break;
                }
                break;
            case 'M':
                if(gnumber == 30) cmd->command = M30;
                break;
            case 'X':
                cmd->x = atoi(&token[1]);
                cmd->has_x = 1;
                break;
            case 'Y':
                cmd->y = atoi(&token[1]);
                cmd->has_y = 1;
                break;
            case 'Z':
                cmd->z = atoi(&token[1]);
                cmd->has_z = 1;
                break;
            case 'F': //probably this part is not needed
                cmd->feedrate = atoi(&token[1]);
                cmd->has_f = 1;
                break;

        }
        token = strtok(NULL, " \t\n");
    }
}


uint8_t compute_flags(const GcodeCommand *cmd)
{
    uint8_t flags = 0;
    if(cmd->has_x) flags |= FLAG_X;
    if(cmd->has_y) flags |= FLAG_Y;
    if(cmd->has_z) flags |= FLAG_Z;
    if(cmd->has_f) flags |= FLAG_F;
    return flags;
}

void steps(const GcodeCommand* cmd, BinaryFlags* flags)
{
    flags->command = (uint8_t)cmd->command;
    flags->flags = compute_flags(cmd);
    flags->x = cmd->has_x ? cmd->x * STEP_PER_MM_X : 0;
    flags->y = cmd->has_y ? cmd->y * STEP_PER_MM_Y : 0;
    flags->z = cmd->has_z ? cmd->z * STEP_PER_MM_Z : 0;
    flags->feedrate = cmd->has_f ? cmd->feedrate : 0;
}

int openSerial(const char* device)
{
    printf("oawngoiawgn\n");
    int fd = open(device, O_RDWR | O_NOCTTY | O_SYNC | O_NONBLOCK);
    printf("tty\n");
    if(fd < 0)
    {
        perror("no serial port for you\n");
        return -1;
    }

    printf("oawngoiawgn\n");

    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    if(tcgetattr(fd, &tty) != 0)
    {
        perror("error with tcgetatttr\n");
        close(fd);
        return -1;
    }

    printf("oawngoiawgn\n");

    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_iflag &= ~IGNBRK;
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_cc[VMIN]  = 1;
    tty.c_cc[VTIME] = 1;

    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    if(tcsetattr(fd,TCSANOW, &tty) != 0)
    {
        perror("something with tcsetattr\n");
        close(fd);
        return -1;
    }

    return fd;
}


int main()
{
    printf("oawngoiawgn\n");
    GcodeCommand cmd;
    BinaryFlags flags;
    char line[128];
    char* filepath = "test.gcode";
    FILE* fp = fopen(filepath, "r");

    printf("oawngoiawgn\n");

    fseek(fp, 0, SEEK_END);
    size_t fp_size = ftell(fp);
    rewind(fp);

    printf("oawngoiawgn\n");

    char *fp_contents = malloc(fp_size+1);
    fread(fp_contents, 1, fp_size, fp);
    fp_contents[fp_size] = 0;

    printf("oawngoiawgn\n");

    fclose(fp);

    printf("oawngoiawgn\n");

    int serial_fd = openSerial("/dev/tty.usbmodem064101C21F221");
    if(serial_fd < 0) return 1;
    int last_x = -1, last_y = -1, last_z = -1, last_feedrate = -1; //feedrate probably not needed but its there just in case
    printf("oawngoiawgn\n");

    while(*fp_contents)
    {
        char *start = fp_contents;
        while (*fp_contents && (*fp_contents)!='\n') ++fp_contents;
        ++fp_contents;

        ptrdiff_t size = fp_contents-start;
        memcpy(line, start, size);
        line[size] = 0;

        /*if(line[0] == ';' || line[0] == '\n') continue;
        parser(line,&cmd);
        if(cmd.command == G90 || cmd.command == G21) continue;
        steps(&cmd,&flags);*/

        printf("oawngoiawgn\n");
        char something;
        while(read(serial_fd, &something, 1) <= 0) printf("waiting\n");
        write(serial_fd, &flags, sizeof(BinaryFlags));
        fsync(serial_fd);

        //checking if any coordinates changed so we don't send the same data:
        /*if((cmd.has_x && cmd.x != last_x) || (cmd.has_y && cmd.y != last_y) || (cmd.has_z && cmd.z != last_z) || (cmd.has_f && cmd.feedrate != last_feedrate))
        {
            last_x = cmd.has_x ? cmd.x : last_x;
            last_y = cmd.has_y ? cmd.y : last_y;
            last_z = cmd.has_z ? cmd.z : last_z;
            last_feedrate = cmd.has_f ? cmd.feedrate : last_feedrate;
            char something;
            printf("oawngoiawgn\n");
            while(read(serial_fd, &something, 1) <= 0) printf("waiting\n");
            write(serial_fd, &flags, sizeof(BinaryFlags));
            fsync(serial_fd);
        }*/
        /* testing
        printf("parsed: G%02d", cmd.command);
        if(cmd.has_x) printf(" x=%d ",cmd.x);
        if(cmd.has_y) printf(" y=%d ",cmd.y);
        if(cmd.has_z) printf(" z=%d ",cmd.z); //probably not needed
        if(cmd.has_f) printf(" f=%d ", cmd.feedrate); //probably not needed
        printf("\n");
        printf("Parsed: G%2d and flags: 0x%02X\n",flags.command,flags.flags);
        */
    }
    //fclose(fp);
    close(serial_fd);
    return 0;

}
