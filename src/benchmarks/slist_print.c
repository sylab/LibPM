#include <cont.h>
#include "slist.h"

int main(int argc, const char *argv[])
{
    struct container *cont = container_restore(0);
    struct slist_head *head = container_getroot(cont->id);

    uint64_t i = 0;
    struct slist_node *node;
    STAILQ_FOREACH(node, &head->head, node) {
        printf("%lu : %.10s...\n", i++, node->data);
    }

    return 0;
}
