/* tree.c - implements tree data structure for standard minecontrol authority programs */
#include "tree.h"
#include <stdlib.h>
#include <string.h>

/* structures used by the implementation */
struct tree_search_info
{
    int index;
    struct tree_node* node;
    const char* key;
};

/* tree_key */
void tree_key_init(struct tree_key* tkey,const char* key,void* payload)
{
    int i;
    int len;
    len = strlen(key)+1;
    tkey->key = malloc(len);
    for (i = 0;i < len;++i)
        tkey->key[i] = key[i];
    tkey->payload = payload;
}
void tree_key_destroy(struct tree_key* tkey)
{
    free(tkey->key);
    if (tkey->payload != NULL)
        free(tkey->payload);
}
static int tree_key_compare_raw(const char* l,const char* r)
{
    while (*l) {
        short i;
        char c[2];
        c[0] = *l;
        c[1] = *r;
        for (i = 0;i < 2;++i)
            if (c[i]>='A' && c[i]<='Z')
                c[i] = c[i]-'A' + 'a';
        if (c[0] != c[1])
            break;
        ++l, ++r;
    }
    return *l - *r;
}
int tree_key_compare(const struct tree_key* left,const struct tree_key* right)
{
    return tree_key_compare_raw(left->key,right->key);
}

/* tree_node */
void tree_node_init(struct tree_node* node)
{
    int i;
    for (i = 0;i < 3;++i)
        node->keys[i] = NULL;
    for (i = 0;i < 4;++i)
        node->children[i] = NULL;
}
void tree_node_destroy(struct tree_node* node)
{
    int i;
    for (i = 0;i < 3;++i) {
        if (node->keys[i] != NULL) {
            tree_key_destroy(node->keys[i]);
            free(node->keys[i]);
        }
    }
    for (i = 0;i < 4;++i) {
        if (node->children[i] != NULL) {
            tree_node_destroy(node->children[i]);
            free(node->children[i]);
        }
    }
}
static int tree_node_insert_key(struct tree_node* node,struct tree_key* key,struct tree_node* left,struct tree_node* right)
{
    int i, j;
    int cmp;
    if (node->keys[0] == NULL)
        cmp = -1;
    else
        cmp = tree_key_compare(key,node->keys[0]);
    if (cmp == 0)
        return 1;
    if (cmp < 0) {
        /* key is inserted into position 0; shift elements over */
        for (i = 1,j = 2;i >= 0;--i,--j)
            node->keys[j] = node->keys[i];
        for (i = 2, j = 3;i >= 0;--i,--j)
            node->children[j] = node->children[i];
        node->keys[0] = key;
        node->children[0] = left;
        node->children[1] = right;
    }
    else if (node->keys[1] != NULL) {
        cmp = tree_key_compare(key,node->keys[1]);
        if (cmp == 0)
            return 1;
        if (cmp < 0) {
            /* key is inserted into position 1; shift elements over */
            node->keys[2] = node->keys[1];
            node->children[3] = node->children[2];
            node->keys[1] = key;
            node->children[1] = left;
            node->children[2] = right;
        }
        else {
            /* key is inserted into position 2; no shift required */
            node->keys[2] = key;
            node->children[2] = left;
            node->children[3] = right;
        }
    }
    else {
        /* key is inserted into position 1; no shift required */
        node->keys[1] = key;
        node->children[1] = left;
        node->children[2] = right;
    }
    return 0;
}
static void tree_node_do_split(struct tree_node* node,struct tree_node* parent)
{
    /* this procedure assumes that the caller has ensured that 'node' is a 4-node */
    int i, j;
    struct tree_key* median;
    struct tree_node* newnode;
    /* the node is already sorted, so choose keys[1] as the median */
    median = node->keys[1];
    /* 'newnode' is going to be the right value; assign children from old node to it; overwrite
       children and keys in 'node' with NULLs so that it becomes a 2-node, thus making 'node'
       the left value */
    newnode = malloc(sizeof(struct tree_node));
    tree_node_init(newnode);
    newnode->keys[0] = node->keys[2];
    for (i = 0,j = 2;i <= 1;++i,++j) {
        newnode->children[i] = node->children[j];
        node->children[j] = NULL;
    }
    for (i = 1;i <= 2;++i)
        node->keys[i] = NULL;
    /* insert the node up into the parent; insertion should succeed because split value should not exist in parent */
    tree_node_insert_key(parent,median,node,newnode);
}
static const char* tree_node_delete_key(struct tree_node* node,const char* key,int index)
{
    /* assume that 'node->keys[index]' exists; this procedure leaves a hole
       in a leave node in some subtree of 'node' to be fixed later; return 
       the search key for the caller if a hole was placed */
    const char* swapKey;
    struct tree_node* n;
    struct tree_key* k;
    swapKey = NULL;
    /* make 'n' point to right subtree; use k to save key to delete */
    n = node->children[index+1];
    k = node->keys[index];
    /* handle case where 'node' is a leaf */
    if (n == NULL) {
        if (node->keys[1] != NULL) {
            /* remove key from 3-node */
            node->keys[index] = node->keys[index+1];
            node->keys[index+1] = NULL;
        }
        else {
            /* leave hole */
            node->keys[index] = NULL;
            swapKey = key;
        }
    }
    /* handle case where 'node' is internal; swap the key to delete with the next-greater key */
    else {
        /* find next-greatest key in tree by searching for smallest element
           of the right subtree; 'n' refers to the root of this subtree */
        while (n->children[0] != NULL)
            n = n->children[0];
        /* if 'n' now points to a leaf node; take its least-ordered key and assign it to the node;
           if 'n' is a 3-node, shift the greater-ordered element over; else if 'n' is a 2-node,
           leave a hole to be fixed later */
        node->keys[index] = n->keys[0];
        if (n->keys[1] != NULL) {
            n->keys[0] = n->keys[1];
            n->keys[1] = NULL;
        }
        else {
            /* leave hole */
            n->keys[0] = NULL;
            swapKey = node->keys[index]->key;
        }
    }
    /* delete the key */
    tree_key_destroy(k);
    free(k);
    return swapKey;
}
static void tree_node_remove_key(struct tree_node* node,int keyIndex,int childIndex)
{
    int i, j;
    for (i = keyIndex,j = keyIndex+1;j < 3;++i,++j)
        node->keys[i] = node->keys[j];
    for (i = childIndex,j = childIndex+1;j < 4;++i,++j)
        node->children[i] = node->children[j];
}
static void tree_node_do_fix(struct tree_node* node,struct tree_node* parent)
{
    /* this procedure assumes that the caller has ensured that 'node' is a hole */
    int i;
    int ni;
    int bound;
    ni = 0;
    while (parent->children[ni] != node)
        ++ni;
    if (parent->keys[1] != NULL)
        bound = 2;
    else
        bound = 1;
    /* see if immediate right sibling is a 3-node */
    if (ni<bound && parent->children[(i = ni+1)]->keys[1] != NULL) {
        int isep = i-1;
        node->keys[0] = parent->keys[isep];
        parent->keys[isep] = parent->children[i]->keys[0];
        node->children[1] = parent->children[i]->children[0];
        tree_node_remove_key(parent->children[i],0,0);
    }
    /* see if immediate left sibling is a 3-node */
    else if (ni>0 && parent->children[(i = ni-1)]->keys[1] != NULL) {
        /* int isep = i; */
        node->keys[0] = parent->keys[i];
        parent->keys[i] = parent->children[i]->keys[1];
        node->children[1] = node->children[0]; /* need to shift this over */
        node->children[0] = parent->children[i]->children[2];
        tree_node_remove_key(parent->children[i],1,1);
    }
    /* any immediate siblings are 2-nodes */
    else {
        int left, right;
        if (ni > 0) {
            left = ni-1;
            right = ni;
        }
        else {
            left = 0;
            right = 1;
        }
        if (parent->children[left]->keys[0] != NULL) {
            /* left is 2-node; right is hole */
            parent->children[left]->keys[1] = parent->keys[left];
            parent->children[left]->children[2] = parent->children[right]->children[0];
        }
        else {
            /* left is hole; right is 2-node */
            parent->children[left]->keys[0] = parent->keys[left];
            parent->children[left]->keys[1] = parent->children[right]->keys[0];
            for (i = 0;i<2;++i)
                parent->children[left]->children[i+1] = parent->children[right]->children[i];
        }
        /* delete the right child node */
        free(parent->children[right]);
        parent->children[right] = NULL;
        /* remove the separator from the parent; shift children over from positions >right */
        tree_node_remove_key(parent,left,right);
        /* parent may now be a hole node */
    }
}

/* search_tree */
void tree_init(struct search_tree* tree)
{
    tree->root = NULL;
}
void tree_destroy(struct search_tree* tree)
{
    if (tree->root != NULL) {
        tree_node_destroy(tree->root);
        free(tree->root);
    }
}
static int tree_insert_recursive(struct tree_node** node,struct tree_node* parent,struct tree_key* key)
{
    struct tree_node* n;
    n = *node;
    if (n->children[0] != NULL) {
        /* 'node' is an internal node, which means it is at least
           a 2-node; recursively search for a leaf */
        static int cmp;
        cmp = tree_key_compare(key,n->keys[0]);
        /* if 'key' already exists, return with error status */
        if (cmp == 0)
            return 1;
        if (cmp < 0) {
            /* 'key' is less than first key in node; visit left subtree */
            if (tree_insert_recursive(n->children,n,key) == 1)
                return 1;
        }
        else if (n->keys[1] != NULL) {
            /* 'node' is a 3-node */
            cmp = tree_key_compare(key,n->keys[1]);
            if (cmp < 0) {
                /* 'key' is less than second key (greater than first); visit middle subtree */
                if (tree_insert_recursive(n->children+1,n,key) == 1)
                    return 1;
            }
            else {
                /* 'key' is greater than first and second keys; visit right subtree */
                if (tree_insert_recursive(n->children+2,n,key) == 1)
                    return 1;
            }
        }
        else {
            /* 'key' is greater than first key; visit right (middle) subtree */
            if (tree_insert_recursive(n->children+1,n,key) == 1)
                return 1;
        }
    }
    /* 'node' is a leaf node; insert the key into it */
    else if (tree_node_insert_key(n,key,NULL,NULL) == 1)
        return 1;
    /* test if 'node' is now a 4-node, in which case it needs to be split */
    if (n->keys[2] != NULL) {
        /* if 'parent' is NULL, then 'node' is the root; we need to create a new root node */
        if (parent == NULL) {
            parent = malloc(sizeof(struct tree_node));
            tree_node_init(parent);
            /* assign the new root */
            *node = parent;
        }
        tree_node_do_split(n,parent);
    }
    /* return success status */
    return 0;
}
int tree_insert(struct search_tree* tree,const char* key,void* payload)
{
    /* return 0 on success, 1 otherwise if the key already exists */
    struct tree_key* keyobj = malloc(sizeof(struct tree_key));
    tree_key_init(keyobj,key,payload);
    /* if the root is null, create the first node */
    if (tree->root == NULL) {
        tree->root = malloc(sizeof(struct tree_node));
        tree_node_init(tree->root);
        tree->root->keys[0] = keyobj;
        return 0;
    }
    /* recursively insert element into tree */
    if (tree_insert_recursive(&tree->root,NULL,keyobj) == 1) {
        /* clean up key object; set payload to NULL so it's not freed */
        keyobj->payload = NULL;
        tree_key_destroy(keyobj);
        return 1;
    }
    return 0;
}
static void tree_search_recursive(struct tree_search_info* info)
{
    static int cmp;
    if (info->node == NULL)
        return;
    cmp = tree_key_compare_raw(info->key,info->node->keys[0]->key);
    if (cmp == 0) {
        /* found key; mark which slot it's in */
        info->index = 0;
        return;
    }
    if (cmp < 0) {
        /* search left-subtree */
        info->node = info->node->children[0];
        tree_search_recursive(info);
        return;
    }
    if (info->node->keys[1] != NULL) {
        /* 'info->node' is a 3-node */
        cmp = tree_key_compare_raw(info->key,info->node->keys[1]->key);
        if (cmp == 0) {
            /* found key; mark which slot it's in */
            info->index = 1;
            return;
        }
        if (cmp < 0) {
            /* search middle-subtree */
            info->node = info->node->children[1];
            tree_search_recursive(info);
            return;
        }
        /* search right-subtree */
        info->node = info->node->children[2];
        tree_search_recursive(info);
        return;
    }
    /* search right-subtree of 2-node */
    info->node = info->node->children[1];
    tree_search_recursive(info);
}
struct tree_key* tree_lookup(struct search_tree* tree,const char* key)
{
    struct tree_search_info info;
    info.node = tree->root;
    info.key = key;
    tree_search_recursive(&info);
    if (info.node != NULL)
        return info.node->keys[info.index];
    return NULL;
}
static void tree_repair_recursive(struct tree_node** node,struct tree_node* parent,const char* key)
{
    /* recursive case: search for the hole */
    if ((*node)->keys[0] != NULL ) {
        if (key == NULL)
            /* search for the hole at the bottom of the left subtree */
            tree_repair_recursive((*node)->children,(*node),key);
        else {
            /* search for the key */
            static int cmp;
            cmp = tree_key_compare_raw(key,(*node)->keys[0]->key);
            if (cmp == 0)
                /* found: search right subtree for successor slot */
                tree_repair_recursive((*node)->children+1,(*node),NULL);
            else if (cmp < 0)
                tree_repair_recursive((*node)->children,(*node),key);
            else if ((*node)->keys[1] != NULL) {
                /* '*node' is a 3-node */
                cmp = tree_key_compare_raw(key,(*node)->keys[1]->key);
                if (cmp == 0)
                    /* found: search right subtree for successor slot */
                    tree_repair_recursive((*node)->children+2,(*node),NULL);
                else if (cmp < 0)
                    tree_repair_recursive((*node)->children+1,(*node),key);
                else
                    tree_repair_recursive((*node)->children+2,(*node),key);
            }
            else
                tree_repair_recursive((*node)->children+1,(*node),key);
        }
        if ((*node)->keys[0] != NULL)
            return;
    }
    if (parent == NULL) {
        /* root node is a hole-node; delete the root node and assign its sole
           child as the new root */
        parent = (*node);
        *node = parent->children[0];
        free(parent);
    }
    else {
        /* base case: fix a hole */
        tree_node_do_fix((*node),parent);
    }
}
int tree_remove(struct search_tree* tree,const char* key)
{
    struct tree_search_info info;
    info.node = tree->root;
    info.key = key;
    tree_search_recursive(&info);
    if (info.node != NULL) {
        const char* skey;
        if ((skey = tree_node_delete_key(info.node,key,info.index)) != NULL)
            /* repair tree if removal left hole in tree */
            tree_repair_recursive(&tree->root,NULL,skey);
        return 0;
    }
    return 1;
}
