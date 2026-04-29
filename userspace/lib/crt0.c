int main(void);
void exit(int status);

void _start(void) 
{
    exit(main());
}
