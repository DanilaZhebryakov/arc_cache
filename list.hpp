template <typename T, typename I> struct SimpleListNode {
    I next;
    I prev;

    T d;

    void remove(SimpleListNode<T,I>* list) {
        list[next].prev = prev;
        list[prev].next = next;
    }
    void insertAfter(SimpleListNode<T,I>* list, I pos) {
        prev = pos;
        next = list[pos].next;
        list[pos].next  = this - list;
        list[next].prev = this - list;
    }
    void insertBefore(SimpleListNode<T,I>* list, I pos) {
        next = pos;
        prev = list[pos].prev;
        list[pos].prev  = this - list;
        list[prev].next = this - list;
    }

    static void init(SimpleListNode<T,I>* list, I start, I end) {
        for (I i = start; i < end; i++) {
            list[i].next = i + 1;
            list[i].prev = i - 1;
        }
        list[start].prev = end - 1;
        list[end-1].next = start;
    }

};
