#include "PriceLevel.hpp"
#include <algorithm>

namespace cmf {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static int height(const PriceLevel* n) { return n ? n->height : 0; }

static void update_height(PriceLevel* n) {
    n->height = 1 + std::max(height(n->left_child), height(n->right_child));
}

static int balance_factor(const PriceLevel* n) {
    return height(n->left_child) - height(n->right_child);
}

// Rotate n to the right; returns the new subtree root.
// Caller must fix the parent's child pointer and the returned node's parent.
static PriceLevel* rotate_right(PriceLevel* n) {
    PriceLevel* l = n->left_child;
    n->left_child = l->right_child;
    if (l->right_child) l->right_child->parent = n;
    l->right_child = n;
    l->parent = n->parent;
    n->parent = l;
    update_height(n);
    update_height(l);
    return l;
}

static PriceLevel* rotate_left(PriceLevel* n) {
    PriceLevel* r = n->right_child;
    n->right_child = r->left_child;
    if (r->left_child) r->left_child->parent = n;
    r->left_child = n;
    r->parent = n->parent;
    n->parent = r;
    update_height(n);
    update_height(r);
    return r;
}

// Restore AVL balance at node n; returns the node now at this position.
static PriceLevel* fix_one(PriceLevel* n) {
    update_height(n);
    int bf = balance_factor(n);
    if (bf > 1) {
        if (balance_factor(n->left_child) < 0) {
            // LR case
            n->left_child = rotate_left(n->left_child);
            n->left_child->parent = n;
        }
        return rotate_right(n);
    }
    if (bf < -1) {
        if (balance_factor(n->right_child) > 0) {
            // RL case
            n->right_child = rotate_right(n->right_child);
            n->right_child->parent = n;
        }
        return rotate_left(n);
    }
    return n;
}

// Walk from 'start' up to the root, rebalancing each node.
// 'old_root' is the tree root before this call; returns the (possibly new) root.
static PriceLevel* rebalance_up(PriceLevel* start, PriceLevel* old_root) {
    PriceLevel* new_root = old_root;
    PriceLevel* cur = start;
    while (cur) {
        PriceLevel* par = cur->parent;
        PriceLevel* nr  = fix_one(cur);
        if (nr != cur) {
            if (par) {
                if (par->left_child == cur) par->left_child = nr;
                else par->right_child = nr;
            }
            nr->parent = par;
        }
        if (!par) new_root = nr;
        cur = par;
    }
    return new_root;
}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

PriceLevel* avl_insert(PriceLevel* root, PriceLevel* node) {
    node->left_child = node->right_child = node->parent = nullptr;
    node->height = 1;

    if (!root) return node;

    PriceLevel* cur = root;
    while (true) {
        if (node->price < cur->price) {
            if (!cur->left_child)  { cur->left_child  = node; node->parent = cur; break; }
            cur = cur->left_child;
        } else {
            if (!cur->right_child) { cur->right_child = node; node->parent = cur; break; }
            cur = cur->right_child;
        }
    }

    return rebalance_up(node->parent, root);
}

PriceLevel* avl_remove(PriceLevel* root, PriceLevel* node) {
    // Two-child case: physically swap node with its in-order successor so that
    // node ends up with at most one child before the simple removal below.
    if (node->left_child && node->right_child) {
        PriceLevel* succ = node->right_child;
        while (succ->left_child) succ = succ->left_child;

        bool direct = (succ == node->right_child);

        PriceLevel* node_par   = node->parent;
        PriceLevel* node_left  = node->left_child;
        PriceLevel* node_right = node->right_child;
        PriceLevel* succ_par   = succ->parent;
        PriceLevel* succ_right = succ->right_child;

        // --- Place succ in node's BST position ---
        succ->parent     = node_par;
        succ->left_child = node_left;
        node_left->parent = succ;

        if (node_par) {
            if (node_par->left_child == node) node_par->left_child = succ;
            else node_par->right_child = succ;
        } else {
            root = succ;
        }

        if (direct) {
            succ->right_child = node;
            node->parent      = succ;
        } else {
            succ->right_child  = node_right;
            node_right->parent = succ;
            // Place node in succ's old BST position (succ was always a left child)
            succ_par->left_child = node;
            node->parent         = succ_par;
        }

        node->left_child  = nullptr;
        node->right_child = succ_right;
        if (succ_right) succ_right->parent = node;

        succ->height = node->height;  // approximate; corrected by rebalance_up
        update_height(node);
    }

    // node now has at most one child (right, since left is nullptr after the swap above,
    // or either child if this was originally a leaf or one-child node).
    PriceLevel* child        = node->right_child ? node->right_child : node->left_child;
    PriceLevel* rebalance_start = node->parent;

    if (node->parent) {
        if (node->parent->left_child == node) node->parent->left_child = child;
        else node->parent->right_child = child;
    } else {
        root = child;
    }
    if (child) child->parent = node->parent;

    // Detach the removed node
    node->parent = node->left_child = node->right_child = nullptr;
    node->height = 1;

    return rebalance_up(rebalance_start, root);
}

} // namespace cmf
