/*Richard James Howe
 * Copy of regex routing from Rob Pike*/
int match(char *regexp, char *text);
int matchhere(char *regexp, char *text);
int matchstar(int c, char *regexp, char *text);
