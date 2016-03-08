#include "rbtree.c"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include <avsystem/commons/log.h>
#include <avsystem/commons/unit/test.h>

static void log_node(void *node) {
    struct rb_node *n = _AVS_RB_NODE(node);
    avs_log(rb, ERROR, "%p: %s, parent %p, left %p, right %p",
            node, n->color == RED ? "RED" : "BLACK",
            n->parent, n->left, n->right);
}

static void dump_tree_recursive(void *node,
                                size_t node_size,
                                void (*print_node)(void *node, size_t size),
                                int level) {
    if (!node) {
        fprintf(stderr, "%*s(null)\n", level * 4, "");
    } else {
        fprintf(stderr, "%*s", level * 4, "");
        print_node(node, node_size);
        fprintf(stderr, "\n");

        if (_AVS_RB_LEFT(node)) {
            AVS_UNIT_ASSERT_TRUE(_AVS_RB_PARENT(_AVS_RB_LEFT(node)) == node);
        }
        if (_AVS_RB_RIGHT(node)) {
            AVS_UNIT_ASSERT_TRUE(_AVS_RB_PARENT(_AVS_RB_RIGHT(node)) == node);
        }
        dump_tree_recursive(_AVS_RB_LEFT(node), node_size, print_node, level + 1);
        dump_tree_recursive(_AVS_RB_RIGHT(node), node_size, print_node, level + 1);
    }
}

static void print_int(void *value,
                      size_t size) {
    (void)size;
    fprintf(stderr, "%d %s", *(int*)value,
            _AVS_RB_NODE(value)->color == RED ? "R" : "B");
}

static void dump_tree(struct rb_tree *tree) {
    dump_tree_recursive(tree->root, sizeof(int), print_int, 0);
}

static void assert_rb_properties_hold_recursive(void *node,
                                                size_t *out_black_height) {
    *out_black_height = 1;
    if (!node) {
        return;
    }

    void *left = _AVS_RB_LEFT(node);
    void *right = _AVS_RB_RIGHT(node);

    size_t left_black_height = 0;
    if (left) {
        AVS_UNIT_ASSERT_TRUE(node == _AVS_RB_PARENT(left));
        assert_rb_properties_hold_recursive(left, &left_black_height);
    }

    size_t right_black_height = 0;
    if (right) {
        AVS_UNIT_ASSERT_TRUE(node == _AVS_RB_PARENT(right));
        assert_rb_properties_hold_recursive(right, &right_black_height);
    }

    AVS_UNIT_ASSERT_EQUAL(left_black_height, right_black_height);
    *out_black_height = left_black_height;

    if (rb_node_color(node) == RED) {
        AVS_UNIT_ASSERT_EQUAL(BLACK, rb_node_color(left));
        AVS_UNIT_ASSERT_EQUAL(BLACK, rb_node_color(right));
    } else {
        ++*out_black_height;
    }
}

static void assert_rb_properties_hold(AVS_RB_TREE(int) tree_) {
    struct rb_tree *tree = _AVS_RB_TREE(tree_);

    AVS_UNIT_ASSERT_EQUAL(BLACK, rb_node_color(tree->root));
    if (tree->root) {
        AVS_UNIT_ASSERT_NULL(_AVS_RB_PARENT(tree->root));
    }

    size_t black_height = 0;
    assert_rb_properties_hold_recursive(tree->root, &black_height);
}

// terminated with 0
static AVS_RB_TREE(int) make_tree(int first, ...) {
    AVS_RB_TREE(int) tree = AVS_RB_TREE_CREATE(int, memcmp);
    AVS_UNIT_ASSERT_NOT_NULL(tree);

    va_list list;
    va_start(list, first);

    int value = first;
    while (value != 0) {
        AVS_RB_NODE(int) elem = AVS_RB_NEW_ELEMENT(int);
        AVS_UNIT_ASSERT_NOT_NULL(elem);

        *elem = value;
        AVS_UNIT_ASSERT_SUCCESS(AVS_RB_TREE_INSERT(tree, elem));

        value = va_arg(list, int);
    }

    va_end(list);

    assert_rb_properties_hold(tree);

    return tree;
}

static void assert_node_equal(int *node,
                              int value,
                              enum rb_color color,
                              int *parent,
                              int *left,
                              int *right) {
    AVS_UNIT_ASSERT_EQUAL(value, *node);
    AVS_UNIT_ASSERT_EQUAL(color, _AVS_RB_NODE(node)->color);
    AVS_UNIT_ASSERT_TRUE(parent == _AVS_RB_PARENT(node));
    AVS_UNIT_ASSERT_TRUE(left == _AVS_RB_LEFT(node));
    AVS_UNIT_ASSERT_TRUE(right == _AVS_RB_RIGHT(node));
}

AVS_UNIT_TEST(rbtree, create) {
    AVS_RB_TREE(int) tree = AVS_RB_TREE_CREATE(int, memcmp);

    struct rb_tree *tree_struct = _AVS_RB_TREE(tree);
    AVS_UNIT_ASSERT_TRUE(tree_struct->cmp == memcmp);
    AVS_UNIT_ASSERT_NULL(tree_struct->root);
}

AVS_UNIT_TEST(rbtree, create_element) {
    AVS_RB_NODE(int) elem = AVS_RB_NEW_ELEMENT(int);

    assert_node_equal(elem, 0, RED, NULL, NULL, NULL);

    AVS_RB_DELETE_ELEMENT(elem);
}

AVS_UNIT_TEST(rbtree, swap_nodes_unrelated) {
    AVS_RB_TREE(int) tree = make_tree(
                      8,
              4,             12,
          2,      6,     10,     14,
        1,  3,  5,  7,  9, 11, 13, 15, 0);

    int *_1 = AVS_RB_TREE_FIND(tree, &(int){1});
    int *_2 = AVS_RB_TREE_FIND(tree, &(int){2});
    int *_3 = AVS_RB_TREE_FIND(tree, &(int){3});
    int *_4 = AVS_RB_TREE_FIND(tree, &(int){4});

    int *_8 =  AVS_RB_TREE_FIND(tree, &(int){8});
    int *_12 = AVS_RB_TREE_FIND(tree, &(int){12});
    int *_10 = AVS_RB_TREE_FIND(tree, &(int){10});
    int *_14 = AVS_RB_TREE_FIND(tree, &(int){14});

    int *a = _2;
    int *b = _12;

    assert_node_equal(a, 2,  BLACK, _4, _1,  _3);
    assert_node_equal(b, 12, RED,   _8, _10, _14);

    swap_nodes(_AVS_RB_TREE(tree), a, b);

    assert_node_equal(a, 2,  RED,   _8, _10, _14);
    assert_node_equal(b, 12, BLACK, _4, _1,  _3);

    AVS_RB_TREE_RELEASE(&tree);
}

AVS_UNIT_TEST(rbtree, swap_nodes_parent_child) {
    AVS_RB_TREE(int) tree = make_tree(
                      8,
              4,             12,
          2,      6,     10,     14,
        1,  3,  5,  7,  9, 11, 13, 15, 0);

    int *_1 = AVS_RB_TREE_FIND(tree, &(int){1});
    int *_2 = AVS_RB_TREE_FIND(tree, &(int){2});
    int *_3 = AVS_RB_TREE_FIND(tree, &(int){3});
    int *_4 = AVS_RB_TREE_FIND(tree, &(int){4});
    int *_6 = AVS_RB_TREE_FIND(tree, &(int){6});
    int *_8 = AVS_RB_TREE_FIND(tree, &(int){8});

    int *a = _2;
    int *b = _4;

    assert_node_equal(a, 2, BLACK, _4, _1, _3);
    assert_node_equal(b, 4, RED,   _8, _2, _6);

    swap_nodes(_AVS_RB_TREE(tree), a, b);

    assert_node_equal(a, 2, RED,   _8, _4, _6);
    assert_node_equal(b, 4, BLACK, _2, _1, _3);

    AVS_RB_TREE_RELEASE(&tree);
}

AVS_UNIT_TEST(rbtree, swap_nodes_parent_child_under_root) {
    AVS_RB_TREE(int) tree = make_tree(
                  4,
              2,      6,
            1,  3,  5,  7, 0);

    int *_1 = AVS_RB_TREE_FIND(tree, &(int){1});
    int *_2 = AVS_RB_TREE_FIND(tree, &(int){2});
    int *_3 = AVS_RB_TREE_FIND(tree, &(int){3});
    int *_4 = AVS_RB_TREE_FIND(tree, &(int){4});
    int *_6 = AVS_RB_TREE_FIND(tree, &(int){6});

    int *a = _2;
    int *b = _4;

    AVS_UNIT_ASSERT_TRUE(_4 == _AVS_RB_TREE(tree)->root);
    assert_node_equal(a, 2, BLACK, _4,   _1, _3);
    assert_node_equal(b, 4, BLACK, NULL, _2, _6);

    swap_nodes(_AVS_RB_TREE(tree), a, b);

    AVS_UNIT_ASSERT_TRUE(_2 == _AVS_RB_TREE(tree)->root);
    assert_node_equal(a, 2, BLACK, NULL, _4, _6);
    assert_node_equal(b, 4, BLACK, _2,   _1, _3);

    AVS_RB_TREE_RELEASE(&tree);
}

AVS_UNIT_TEST(rbtree, rotate_left) {
    AVS_RB_TREE(int) tree = make_tree(3, 2, 5, 7, 4, 0);
    //          3B
    //     2B         5B
    //    *  *     4R    7R
    //            *  *  *  *

    int *_3 = AVS_RB_TREE_FIND(tree, &(int){3});
    int *_2 = AVS_RB_TREE_FIND(tree, &(int){2});
    int *_5 = AVS_RB_TREE_FIND(tree, &(int){5});
    int *_4 = AVS_RB_TREE_FIND(tree, &(int){4});
    int *_7 = AVS_RB_TREE_FIND(tree, &(int){7});

    assert_node_equal(_3, 3, BLACK, NULL, _2,   _5);
    assert_node_equal(_2, 2, BLACK, _3,   NULL, NULL);
    assert_node_equal(_5, 5, BLACK, _3,   _4,   _7);
    assert_node_equal(_4, 4, RED,   _5,   NULL, NULL);
    assert_node_equal(_7, 7, RED,   _5,   NULL, NULL);

    rb_rotate_left(_AVS_RB_TREE(tree), _3);

    // should be:
    //          5B
    //     3B        7R
    //  2B    4R   *    *
    // *  *  *  *

    AVS_UNIT_ASSERT_TRUE(_5 == _AVS_RB_TREE(tree)->root);
    assert_node_equal(_5, 5, BLACK, NULL, _3,   _7);
    assert_node_equal(_3, 3, BLACK, _5,   _2,   _4);
    assert_node_equal(_7, 7, RED,   _5,   NULL, NULL);
    assert_node_equal(_2, 2, BLACK, _3,   NULL, NULL);
    assert_node_equal(_4, 4, RED,   _3,   NULL, NULL);

    AVS_RB_TREE_RELEASE(&tree);
}

AVS_UNIT_TEST(rbtree, rotate_right) {
    AVS_RB_TREE(int) tree = make_tree(5, 3, 7, 2, 4, 0);
    //          5B
    //     3B        7B
    //  2R    4R   *    *
    // *  *  *  *

    int *_5 = AVS_RB_TREE_FIND(tree, &(int){5});
    int *_3 = AVS_RB_TREE_FIND(tree, &(int){3});
    int *_7 = AVS_RB_TREE_FIND(tree, &(int){7});
    int *_2 = AVS_RB_TREE_FIND(tree, &(int){2});
    int *_4 = AVS_RB_TREE_FIND(tree, &(int){4});

    assert_node_equal(_5, 5, BLACK, NULL, _3,   _7);
    assert_node_equal(_3, 3, BLACK, _5,   _2,   _4);
    assert_node_equal(_7, 7, BLACK, _5,   NULL, NULL);
    assert_node_equal(_2, 2, RED,   _3,   NULL, NULL);
    assert_node_equal(_4, 4, RED,   _3,   NULL, NULL);

    rb_rotate_right(_AVS_RB_TREE(tree), _5);

    // should be:
    //          3B
    //     2R         5B
    //    *  *     4R    7B
    //            *  *  *  *

    AVS_UNIT_ASSERT_TRUE(_3 == _AVS_RB_TREE(tree)->root);
    assert_node_equal(_3, 3, BLACK, NULL, _2,   _5);
    assert_node_equal(_2, 2, RED,   _3,   NULL, NULL);
    assert_node_equal(_5, 5, BLACK, _3,   _4,   _7);
    assert_node_equal(_7, 7, BLACK, _5,   NULL, NULL);
    assert_node_equal(_4, 4, RED,   _5,   NULL, NULL);

    AVS_RB_TREE_RELEASE(&tree);
}

AVS_UNIT_TEST(rbtree, insert_case1_first) {
    int **tree = AVS_RB_TREE_CREATE(int, memcmp);
    int *elem = AVS_RB_NEW_ELEMENT(int);

    AVS_UNIT_ASSERT_SUCCESS(AVS_RB_TREE_INSERT(tree, elem));

    AVS_UNIT_ASSERT_TRUE(_AVS_RB_TREE(tree)->cmp == memcmp);
    AVS_UNIT_ASSERT_TRUE(_AVS_RB_TREE(tree)->root == elem);

    assert_node_equal(elem, 0, BLACK, NULL, NULL, NULL);

    assert_rb_properties_hold(tree);

    AVS_RB_TREE_RELEASE(&tree);
}

AVS_UNIT_TEST(rbtree, insert_case2) {
    AVS_RB_TREE(int) tree = make_tree(2, 0);
    //   2
    // *   *

    int *elem = AVS_RB_NEW_ELEMENT(int);
    *elem = 1;
    AVS_UNIT_ASSERT_SUCCESS(AVS_RB_TREE_INSERT(tree, elem));

    // should be:
    //    2
    //  1   *
    // * *

    void *root = _AVS_RB_TREE(tree)->root;
    AVS_UNIT_ASSERT_TRUE(root == _AVS_RB_PARENT(elem));
    AVS_UNIT_ASSERT_NULL(_AVS_RB_LEFT(elem));
    AVS_UNIT_ASSERT_NULL(_AVS_RB_RIGHT(elem));
    AVS_UNIT_ASSERT_EQUAL(BLACK, _AVS_RB_NODE(root)->color);

    AVS_UNIT_ASSERT_NULL(_AVS_RB_PARENT(root));
    AVS_UNIT_ASSERT_TRUE(elem == _AVS_RB_LEFT(root));
    AVS_UNIT_ASSERT_NULL(_AVS_RB_RIGHT(root));
    AVS_UNIT_ASSERT_EQUAL(RED, _AVS_RB_NODE(elem)->color);

    assert_rb_properties_hold(tree);

    AVS_RB_TREE_RELEASE(&tree);
}

AVS_UNIT_TEST(rbtree, insert_case3) {
    AVS_RB_TREE(int) tree = make_tree(5, 2, 7, 0);
    //     5
    //  2     7
    // * *   * *

    int *_5 = AVS_RB_TREE_FIND(tree, &(int){5});
    int *_2 = AVS_RB_TREE_FIND(tree, &(int){2});
    int *_7 = AVS_RB_TREE_FIND(tree, &(int){7});

    int *_1 = AVS_RB_NEW_ELEMENT(int);
    *_1 = 1;
    AVS_UNIT_ASSERT_SUCCESS(AVS_RB_TREE_INSERT(tree, _1));

    // should be:
    //          5
    //     2         7
    //  1     *     * *
    // * *

    AVS_UNIT_ASSERT_TRUE(_5 == _AVS_RB_TREE(tree)->root);

    assert_node_equal(_5, 5, BLACK, NULL, _2,   _7);
    assert_node_equal(_2, 2, BLACK, _5,   _1,   NULL);
    assert_node_equal(_1, 1, RED,   _2,   NULL, NULL);
    assert_node_equal(_7, 7, BLACK, _5,   NULL, NULL);

    assert_rb_properties_hold(tree);

    AVS_RB_TREE_RELEASE(&tree);
}

AVS_UNIT_TEST(rbtree, insert_case4_5) {
    AVS_RB_TREE(int) tree = make_tree(5, 4, 7, 2, 0);
    //          5B
    //     4B        7B
    //  2R    *    *    *
    // *  *

    int *_5 = AVS_RB_TREE_FIND(tree, &(int){5});
    int *_4 = AVS_RB_TREE_FIND(tree, &(int){4});
    int *_7 = AVS_RB_TREE_FIND(tree, &(int){7});
    int *_2 = AVS_RB_TREE_FIND(tree, &(int){2});

    AVS_UNIT_ASSERT_TRUE(_5 == _AVS_RB_TREE(tree)->root);

    assert_node_equal(_5, 5, BLACK, NULL, _4,   _7);
    assert_node_equal(_4, 4, BLACK, _5,   _2,   NULL);
    assert_node_equal(_7, 7, BLACK, _5,   NULL, NULL);
    assert_node_equal(_2, 2, RED,   _4,   NULL, NULL);

    int *_3 = AVS_RB_NEW_ELEMENT(int);
    *_3 = 3;
    AVS_UNIT_ASSERT_SUCCESS(AVS_RB_TREE_INSERT(tree, _3));

    // should be:
    //            5B
    //      3B          7B
    //   2R    4R      *  *
    //  *  *  *  *

    AVS_UNIT_ASSERT_TRUE(_5 == _AVS_RB_TREE(tree)->root);
    assert_node_equal(_5, 5, BLACK, NULL, _3,   _7);
    assert_node_equal(_3, 3, BLACK, _5,   _2,   _4);
    assert_node_equal(_7, 7, BLACK, _5,   NULL, NULL);
    assert_node_equal(_2, 2, RED,   _3,   NULL, NULL);
    assert_node_equal(_4, 4, RED,   _3,   NULL, NULL);

    assert_rb_properties_hold(tree);

    AVS_RB_TREE_RELEASE(&tree);
}

static AVS_RB_TREE(int) make_full_3level_tree(void) {
    AVS_RB_TREE(int) tree = make_tree(
                4,
            2,      6,
          1,  3,  5,  7, 0);

    int *_4 = AVS_RB_TREE_FIND(tree, &(int){4});
    int *_2 = AVS_RB_TREE_FIND(tree, &(int){2});
    int *_1 = AVS_RB_TREE_FIND(tree, &(int){1});
    int *_3 = AVS_RB_TREE_FIND(tree, &(int){3});
    int *_6 = AVS_RB_TREE_FIND(tree, &(int){6});
    int *_5 = AVS_RB_TREE_FIND(tree, &(int){5});
    int *_7 = AVS_RB_TREE_FIND(tree, &(int){7});

    AVS_UNIT_ASSERT_TRUE(_4 == _AVS_RB_TREE(tree)->root);
    assert_node_equal(_4, 4, BLACK, NULL, _2,  _6);
    assert_node_equal(_2, 2, BLACK, _4,   _1,   _3);
    assert_node_equal(_1, 1, RED,   _2,   NULL, NULL);
    assert_node_equal(_3, 3, RED,   _2,   NULL, NULL);
    assert_node_equal(_6, 6, BLACK, _4,   _5,   _7);
    assert_node_equal(_5, 5, RED,   _6,   NULL, NULL);
    assert_node_equal(_7, 7, RED,   _6,   NULL, NULL);

    assert_rb_properties_hold(tree);

    return tree;
}

AVS_UNIT_TEST(rbtree, traverse_forward) {
    AVS_RB_TREE(int) tree = make_full_3level_tree();

    int *node = AVS_RB_TREE_FIRST(tree);
    for (int i = 1; i <= 7; ++i) {
        AVS_UNIT_ASSERT_EQUAL(i, *node);
        node = AVS_RB_NEXT(node);
    }

    AVS_UNIT_ASSERT_NULL(node);

    AVS_RB_TREE_RELEASE(&tree);
}

AVS_UNIT_TEST(rbtree, traverse_backward) {
    AVS_RB_TREE(int) tree = make_full_3level_tree();

    int *node = AVS_RB_TREE_LAST(tree);
    for (int i = 7; i >= 1; --i) {
        AVS_UNIT_ASSERT_EQUAL(i, *node);
        node = AVS_RB_PREV(node);
    }

    AVS_UNIT_ASSERT_NULL(node);

    AVS_RB_TREE_RELEASE(&tree);
}

AVS_UNIT_TEST(rbtree, detach_root) {
    AVS_RB_TREE(int) tree = make_tree(1, 0);
    //   1B
    //  *  *

    int *_1 = AVS_RB_TREE_FIND(tree, &(int){1});
    assert_node_equal(_1, 1, BLACK, NULL, NULL, NULL);
    assert_rb_properties_hold(tree);

    AVS_UNIT_ASSERT_TRUE(_1 == AVS_RB_TREE_DETACH(tree, _1));
    // should be:
    //   *

    assert_node_equal(_1, 1, BLACK, NULL, NULL, NULL);
    AVS_UNIT_ASSERT_NULL(_AVS_RB_TREE(tree)->root);
    assert_rb_properties_hold(tree);

    AVS_RB_DELETE_ELEMENT(_1);
    AVS_RB_TREE_RELEASE(&tree);
}

AVS_UNIT_TEST(rbtree, detach_single_root_child) {
    AVS_RB_TREE(int) tree = make_tree(1, 2, 0);
    //  1B
    // *  2R

    assert_rb_properties_hold(tree);

    int *_1 = AVS_RB_TREE_FIND(tree, &(int){1});
    int *_2 = AVS_RB_TREE_FIND(tree, &(int){2});

    assert_node_equal(_1, 1, BLACK, NULL, NULL, _2);
    assert_node_equal(_2, 2, RED,   _1,   NULL, NULL);

    AVS_UNIT_ASSERT_TRUE(_2 == AVS_RB_TREE_DETACH(tree, _2));
    // should be:
    //  1B
    // *  *

    assert_node_equal(_1, 1, BLACK, NULL, NULL, NULL);
    AVS_UNIT_ASSERT_TRUE(_1 == _AVS_RB_TREE(tree)->root);
    assert_rb_properties_hold(tree);

    assert_node_equal(_2, 2, RED, NULL, NULL, NULL);

    AVS_RB_DELETE_ELEMENT(_2);
    AVS_RB_TREE_RELEASE(&tree);
}

AVS_UNIT_TEST(rbtree, fuzz1) {
    AVS_RB_TREE(int) tree = make_tree(3, 1, 2, 0);
    assert_rb_properties_hold(tree);

    int *_2 = AVS_RB_TREE_FIND(tree, &(int){2});
    AVS_UNIT_ASSERT_TRUE(_2 == AVS_RB_TREE_DETACH(tree, _2));
    AVS_RB_DELETE_ELEMENT(_2);

    assert_rb_properties_hold(tree);

    AVS_RB_TREE_RELEASE(&tree);
}

AVS_UNIT_TEST(rbtree, fuzz2) {
    AVS_RB_TREE(int) tree = make_tree(2, 5, 3, 1, 0);
    assert_rb_properties_hold(tree);

    int *_3 = AVS_RB_TREE_FIND(tree, &(int){3});
    AVS_UNIT_ASSERT_TRUE(_3 == AVS_RB_TREE_DETACH(tree, _3));
    AVS_RB_DELETE_ELEMENT(_3);

    assert_rb_properties_hold(tree);

    AVS_RB_TREE_RELEASE(&tree);
}
