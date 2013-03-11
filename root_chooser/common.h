//print helpers
#define FATAL(x,args...)	do{printf("[FATAL] "x,##args);printed_lines++;fatal_error=1;}while(0)
#define ERROR(x,args...) 	do{printf("[ERROR] "x,##args);printed_lines++;}while(0)
#define WARN(x,args...)		do{printf("[WARN ] "x,##args);printed_lines++;}while(0)
#define INFO(x,args...)		do{printf("[INFO ] "x,##args);printed_lines++;}while(0)
#if 1
#define DEBUG(x,args...) 	do{printf("[DEBUG] "x,##args);printed_lines++;}while(0)
#define STOP_BEFORE_MENU
#else
#define DEBUG(x,args...)
#endif

#define MAX_LINE 255

extern int printed_lines;
extern int fatal_error;
