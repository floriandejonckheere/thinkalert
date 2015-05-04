/*---------------------------------------------------------------------------
 * ThinkAlert - A small program to manipulate the ThinkLight.
 *    Based on a "stupid little hack to blink the ThinkLight"
 *    http://paste.lisp.org/display/37500
 *
 * New features:
 *    - Interval argument is optional
 *    - Turn the light on or off (and leave it that way)
 *    - Specify separate values for on/off periods
 *    - Restores the light back to its initial state before terminating
 *    - Drops privileges after opening ThinkLight interface
 *
 *-------------------------------------------------------------------------*/
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>
#include <unistd.h>

void blink(int, int, int);
void dropPrivs();
void lightOn();
void lightOff();
void restoreState();
void saveState();
void terminate(int);

const char filename[] = "/proc/acpi/ibm/light";
int initialState = 0;   // Initial state of the ThinkLight
FILE *thinklight;                       // Handle to the ThinkLight proc interface.

#define DEFAULT_ON_PERIOD               250000
#define DEFAULT_OFF_PERIOD              250000


// ThinkAlert
int main(int argc, char *argv[]) {

        int noBlink = 0;       // Whether light should be blinked or switched
        int onPeriod;          // Duration of a blink (microseconds)
        int offPeriod;         // Interval between blinks (microseconds)
        int switchOn = 0;      // Whether light should be switched on or off
        int times;                     // Number of times to blink

        // Attempt to open the ThinkLight interface.
        thinklight = fopen(filename, "r+");
        if (!thinklight) {
                perror("Unable to open the ThinkLight interface");
                return errno;
        }
        dropPrivs();

        switch (argc - 1) {
                case 1:
                        if (!strcmp(argv[1], "on")) {
                                noBlink = 1;
                                switchOn = 1;
                        } else if (!strcmp(argv[1], "off")) noBlink = 1;
                        else {
                                times = atoi(argv[1]);
                                onPeriod = DEFAULT_ON_PERIOD;
                                offPeriod = DEFAULT_OFF_PERIOD;
                        }
                        break;
                case 2:
                        times = atoi(argv[1]);
                        onPeriod = offPeriod = atoi(argv[2]);
                        break;
                case 3:
                        times = atoi(argv[1]);
                        onPeriod = atoi(argv[2]);
                        offPeriod = atoi(argv[3]);
                        break;
                default:
                        printf("thinkalert <on|off>\n");
                        printf("thinkalert <times> [interval (microseconds)]\n");
                        printf("thinkalert <times> <on period (microseconds)> "
                                "<off period (microseconds)>\n");
                        fclose(thinklight);
                        exit(0);
        }

        // Just turn the light on or off and leave it that way.
        if (noBlink) {
                if (switchOn) lightOn();
                else lightOff();
                fclose(thinklight);
        }

        // Blink the light.
        else {
                saveState();
                signal(SIGINT, terminate);
                blink(times, onPeriod, offPeriod);
                terminate(0);
        }
        return 0;
}


// Blink the light a number of times.
void blink(int times, int onPeriod, int offPeriod) {

        if (!times) return;            // Blink zero times.

        int t = times;
        while (t--) {

                // Only shine the first time if the light was initially off.
                if ((t < (times - 1)) || !initialState) {
                        lightOn();
                        usleep(onPeriod);
                }

                // Only shade the last time if the light was initially on.
                if (t || initialState) {
                        lightOff();
                        usleep(offPeriod);
                }
        }
}


// Drop root privileges.  This code is Linux specific.  Adapted from Secure
// Programming Cookbook for C and C++.
void dropPrivs() {
        gid_t newgid = getgid(), oldgid = getegid();
        uid_t newuid = getuid(), olduid = geteuid();

        // Drop ancillary group memberships.
        if (!olduid) setgroups(1, &newgid);

        // Set the effective gid to the real gid.
        if ((newgid != oldgid) && (-1 == setregid(newgid, newgid))) abort();

        // Set the effective uid to the real uid.
        if ((newuid != olduid) && (-1 == setreuid(newuid, newuid))) abort();

        // Verify that the changes were successful.
        if (newgid != oldgid && (-1 != setegid(oldgid) || getegid() != newgid))
                abort();
        if (newuid != olduid && (-1 != seteuid(olduid) || geteuid() != newuid))
                abort();
}


// Turn the light on.
void lightOn() {
        rewind(thinklight);
        fprintf(thinklight, "on");
        fflush(thinklight);
}


// Turn the light off.
void lightOff() {
        rewind(thinklight);
        fprintf(thinklight, "off");
        fflush(thinklight);
}


// Restore the initial state of the ThinkLight.
void restoreState() {
        if (initialState) lightOn();
        else lightOff();
}


// Get the initial state of the ThinkLight.
void saveState() {
        char status;
        fseek(thinklight, 10, SEEK_SET);
        fread(&status, 1, 1, thinklight);
        if ('n' == status) initialState = 1;
}


// Terminate the program.
void terminate(int sig) {
        restoreState();
        fclose(thinklight);
        exit(sig);
}
