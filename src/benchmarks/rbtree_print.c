#include <cont.h>
#include "rbtree.h"

int main(int argc, const char *argv[])
{
    struct container *cont = container_restore(0);
    struct rbroot *root = container_getroot(cont->id);
    rbroot_print(root);
    return 0;
}
