#include <stdio.h>
#include <string.h>
#include <windows.h>

int main() {
    char line[1024];
    
    // Print immediately
    printf("Echo server ready\n");
    fflush(stdout);
    
    printf("> ");
    fflush(stdout);
    
    while (fgets(line, sizeof(line), stdin)) {
        // Remove trailing newline if present
        int len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }
        
        printf("You said: %s\n", line);
        printf("> ");
        fflush(stdout);
    }
    
    printf("Goodbye\n");
    fflush(stdout);
    
    return 0;
} 