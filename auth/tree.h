/* tree.h - provides search tree data structure for 
   minecontrol authority programs; keys are case-insensitive
   strings */
#ifndef MINECONTROLAUTH_TREE_H
#define MINECONTROLAUTH_TREE_H

struct tree_key
{
    /* store key and payload on heap */
    char* key;
    void* payload;
};
void tree_key_init(struct tree_key* tkey,const char* key,void* payload);
void tree_key_destroy(struct tree_key* tkey);
/* return <0 if left<right, return 0 if left==right, return >0 if left>right */
int tree_key_compare(const struct tree_key* left,const struct tree_key* right);

struct tree_node
{
    /* store keys and children on heap */
    struct tree_key* keys[3];
    struct tree_node* children[4];
};
void tree_node_init(struct tree_node* node);
void tree_node_destroy(struct tree_node* node);

struct search_tree
{
    /* store root on heap */
    struct tree_node* root;
};
void tree_init(struct search_tree* tree);
void tree_construct(struct search_tree* tree,struct tree_key* keys,int size);
void tree_destroy(struct search_tree* tree);
/* return 1 if key already exists in tree */
int tree_insert(struct search_tree* tree,const char* key,void* payload);
/* return NULL if lookup failed */
struct tree_key* tree_lookup(struct search_tree* tree,const char* key);
/* return 1 if removal failed */
int tree_remove(struct search_tree* tree,const char* key);

#endif
