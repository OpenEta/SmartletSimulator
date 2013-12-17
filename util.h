
#ifndef UTIL_
#define UTIL_

#ifndef bool
typedef int bool;
#endif
#define true 1
#define false 0

#define QUIT() raise(SIGINT);

// Define logging macros with different levels
#define LOGINF(level, message, ...) \
    do if(level <= verbose){ printf(message, ##__VA_ARGS__); fflush (stdout); } while (0)
#define LOGERR(message, ...) \
    do { fprintf(stderr,message, ##__VA_ARGS__); fflush (stderr); } while (0)
#define LOGERRNO(funcerr,  message, ...) \
    do{ fprintf(stderr, "ERROR! in %s - ", __func__); fprintf(stderr, message, ##__VA_ARGS__); perror(funcerr);} while (0)
#define FATALERR(message, ...) \
    do{ fprintf(stderr, "FATAL ERROR! in \"%s\" - ", __func__); fprintf(stderr, message, ##__VA_ARGS__); fflush(stderr); QUIT();} while (0)
#define FATALERRNO(funcerr, message, ...) \
    do{ fprintf(stderr, "FATAL ERROR! in \"%s\" - ", __func__); fprintf(stderr, message, ##__VA_ARGS__); perror(funcerr); QUIT();} while (0)

#endif
