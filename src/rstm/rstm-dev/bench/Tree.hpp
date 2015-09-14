/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#ifndef TREE_HPP__
#define TREE_HPP__

#include <climits>
#include <api/api.hpp> // need this for malloc and free

class RBTree
{
    enum Color { RED, BLACK };

    // Node of an RBTree
    struct RBNode
    {
        Color   m_color;
        int     m_val;
        RBNode* m_parent;
        int     m_ID;
        RBNode* m_child[2];

        // basic constructor
        RBNode(Color color = BLACK,
               long val = -1,
               RBNode* parent = NULL,
               long ID = 0,
               RBNode* child0 = NULL,
               RBNode* child1 = NULL)
            : m_color(color), m_val(val), m_parent(parent), m_ID(ID)
        {
            m_child[0] = child0;
            m_child[1] = child1;
        }
    };

    // helper functions for sanity checks
    static int blackHeight(const RBNode* x);
    static bool redViolation(const RBNode* p_r, const RBNode* x);
    static bool validParents(const RBNode* p, int xID, const RBNode* x);
    static bool inOrder(const RBNode* x, int lowerBound, int upperBound);

  public:
    RBNode* sentinel;

    RBTree();

    // standard IntSet methods
    TM_CALLABLE
    bool lookup(int val TM_ARG) const;

    TM_CALLABLE
    void insert(int val TM_ARG);

    TM_CALLABLE
    void remove(int val TM_ARG);

    TM_CALLABLE
    void modify(int val TM_ARG);

    bool isSane() const;
};

// binary search for the node that has v as its value
bool RBTree::lookup(int v TM_ARG) const
{
    // find v
    RBNode* x = TM_READ(sentinel->m_child[0]);
    while (x != NULL) {
        long xval = TM_READ(x->m_val);
        if (xval == v)
            return true;
        else
            x = TM_READ(x->m_child[(v < xval) ? 0 : 1]);
    }
    return false;
}

void RBTree::modify(int v TM_ARG)
{
    if (lookup(v TM_PARAM))
        remove(v TM_PARAM);
    else
        insert(v TM_PARAM);
}

// insert a node with v as its value if no such node exists in the tree
void RBTree::insert(int v TM_ARG)
{
    // find insertion point
    const RBNode* curr(sentinel);
    int cID = 0;
    RBNode* child(TM_READ(curr->m_child[cID]));

    while (child != NULL) {
        long cval = TM_READ(child->m_val);
        if (cval == v)
            return; // don't add existing key
        cID = v < cval ? 0 : 1;
        curr = child;
        child = TM_READ(curr->m_child[cID]);
    }

    // create the new node ("child") and attach it as curr->child[cID]
    child = (RBNode*)TM_ALLOC(sizeof(RBNode));
    child->m_color = RED;
    child->m_val = v;
    child->m_parent = const_cast<RBNode*>(curr);
    child->m_ID = cID;
    child->m_child[0] = NULL;
    child->m_child[1] = NULL;

    RBNode* curr_rw = const_cast<RBNode*>(curr);
    const RBNode* child_r(child);
    TM_WRITE(curr_rw->m_child[cID], child);

    // balance the tree
    while (true) {
        const RBNode* parent_r(TM_READ(child_r->m_parent));
        // read the parent of curr as gparent
        const RBNode* gparent_r(TM_READ(parent_r->m_parent));

        if ((gparent_r == sentinel) ||
            (BLACK == TM_READ(parent_r->m_color)))
            break;

        // cache the ID of the parent
        int pID = TM_READ(parent_r->m_ID);
        // get parent's sibling as aunt
        const RBNode* aunt_r(TM_READ(gparent_r->m_child[1-pID]));
        // gparent and parent will be written on all control paths
        RBNode* gparent_w = const_cast<RBNode*>(gparent_r);
        RBNode* parent_w = const_cast<RBNode*>(parent_r);

        if ((aunt_r != NULL) && (RED == TM_READ(aunt_r->m_color))) {
            // set parent and aunt to BLACK, grandparent to RED
            TM_WRITE(parent_w->m_color, BLACK);
            RBNode* aunt_rw = const_cast<RBNode*>(aunt_r);
            TM_WRITE(aunt_rw->m_color, BLACK);
            TM_WRITE(gparent_w->m_color, RED);
            // now restart loop at gparent level
            child_r = gparent_w;
            continue;
        }

        int cID = TM_READ(child_r->m_ID);
        if (cID != pID) {
            // promote child
            RBNode* child_rw = const_cast<RBNode*>(child_r);
            RBNode* baby(TM_READ(child_rw->m_child[1-cID]));
            // set child's child to parent's cID'th child
            TM_WRITE(parent_w->m_child[cID], baby);
            if (baby != NULL) {
                RBNode* baby_w(baby);
                TM_WRITE(baby_w->m_parent, parent_w);
                TM_WRITE(baby_w->m_ID, cID);
            }
            // move parent into baby's position as a child of child
            TM_WRITE(child_rw->m_child[1-cID], parent_w);
            TM_WRITE(parent_w->m_parent, child_rw);
            TM_WRITE(parent_w->m_ID, 1-cID);
            // move child into parent's spot as pID'th child of gparent
            TM_WRITE(gparent_w->m_child[pID], child_rw);
            TM_WRITE(child_rw->m_parent, gparent_w);
            TM_WRITE(child_rw->m_ID, pID);
            // promote(child_rw);
            // now swap child with curr and continue
            const RBNode* t(child_rw);
            child_r = parent_w;
            parent_w = const_cast<RBNode*>(t);
        }

        TM_WRITE(parent_w->m_color, BLACK);
        TM_WRITE(gparent_w->m_color, RED);
        // promote parent
        RBNode* ggparent_w(TM_READ(gparent_w->m_parent));
        int gID = TM_READ(gparent_w->m_ID);
        RBNode* ochild = TM_READ(parent_w->m_child[1 - pID]);
        // make gparent's pIDth child ochild
        TM_WRITE(gparent_w->m_child[pID], ochild);
        if (ochild != NULL) {
            RBNode* ochild_w(ochild);
            TM_WRITE(ochild_w->m_parent, gparent_w);
            TM_WRITE(ochild_w->m_ID, pID);
        }
        // make gparent the 1-pID'th child of parent
        TM_WRITE(parent_w->m_child[1-pID], gparent_w);
        TM_WRITE(gparent_w->m_parent, parent_w);
        TM_WRITE(gparent_w->m_ID, 1-pID);
        // make parent the gIDth child of ggparent
        TM_WRITE(ggparent_w->m_child[gID], parent_w);
        TM_WRITE(parent_w->m_parent, ggparent_w);
        TM_WRITE(parent_w->m_ID, gID);
        // promote(parent_w);
    }

    // now just set the root to black
    const RBNode* sentinel_r(sentinel);
    const RBNode* root_r(TM_READ(sentinel_r->m_child[0]));
    if (TM_READ(root_r->m_color) != BLACK) {
        RBNode* root_rw = const_cast<RBNode*>(root_r);
        TM_WRITE(root_rw->m_color, BLACK);
    }
}

// remove the node with v as its value if it exists in the tree
void RBTree::remove(int v TM_ARG)
{
    // find v
    const RBNode* sentinel_r(sentinel);
    // rename x_r to x_rw, x_rr to x_r
    const RBNode* x_r(TM_READ(sentinel_r->m_child[0]));

    while (x_r != NULL) {
        int xval = TM_READ(x_r->m_val);
        if (xval == v)
            break;
        x_r = TM_READ(x_r->m_child[v < xval ? 0 : 1]);
    }

    // if we found v, remove it.  Otherwise return
    if (x_r == NULL)
        return;

    RBNode* x_rw = const_cast<RBNode*>(x_r); // upgrade to writable

    // ensure that we are deleting a node with at most one child
    // cache value of rhs child
    RBNode* xrchild(TM_READ(x_rw->m_child[1]));
    if ((xrchild != NULL) && (TM_READ(x_rw->m_child[0]) != NULL)) {
        // two kids!  find right child's leftmost child and swap it
        // with x
        const RBNode* leftmost_r(TM_READ(x_rw->m_child[1]));

        while (TM_READ(leftmost_r->m_child[0]) != NULL)
            leftmost_r = TM_READ(leftmost_r->m_child[0]);

        TM_WRITE(x_rw->m_val, TM_READ(leftmost_r->m_val));
        x_rw = const_cast<RBNode*>(leftmost_r);
    }

    // extract x from the tree and prep it for deletion
    RBNode* parent_rw(TM_READ(x_rw->m_parent));
    int cID = (TM_READ(x_rw->m_child[0]) != NULL) ? 0 : 1;
    RBNode* child = TM_READ(x_rw->m_child[cID]);
    // make child the xID'th child of parent
    int xID = TM_READ(x_rw->m_ID);
    TM_WRITE(parent_rw->m_child[xID], child);
    if (child != NULL) {
        RBNode* child_w(child);
        TM_WRITE(child_w->m_parent, parent_rw);
        TM_WRITE(child_w->m_ID, xID);
    }

    // fix black height violations
    if ((BLACK == TM_READ(x_rw->m_color)) && (child != NULL)) {
        const RBNode* c_r(child);
        if (RED == TM_READ(c_r->m_color)) {
            RBNode* c_rw = const_cast<RBNode*>(c_r);
            TM_WRITE(x_rw->m_color, RED);
            TM_WRITE(c_rw->m_color, BLACK);
        }
    }

    // rebalance
    RBNode* curr(x_rw);
    while (true) {
        parent_rw = TM_READ(curr->m_parent);
        if ((parent_rw == sentinel) || (RED == TM_READ(curr->m_color)))
            break;
        int cID = TM_READ(curr->m_ID);
        RBNode* sibling_w(TM_READ(parent_rw->m_child[1 - cID]));

        // we'd like y's sibling s to be black
        // if it's not, promote it and recolor
        if (RED == TM_READ(sibling_w->m_color)) {
            /*
                   Bp          Bs
                  / \         / \
                 By  Rs  =>  Rp  B2
                 / \     / \
                B1 B2  By  B1
            */
            TM_WRITE(parent_rw->m_color, RED);
            TM_WRITE(sibling_w->m_color, BLACK);
            // promote sibling
            RBNode* gparent_w(TM_READ(parent_rw->m_parent));
            int pID = TM_READ(parent_rw->m_ID);
            RBNode* nephew_w(TM_READ(sibling_w->m_child[cID]));
            // set nephew as 1-cID child of parent
            TM_WRITE(parent_rw->m_child[1-cID], nephew_w);
            TM_WRITE(nephew_w->m_parent, parent_rw);
            TM_WRITE(nephew_w->m_ID, (1-cID));
            // make parent the cID child of the sibling
            TM_WRITE(sibling_w->m_child[cID], parent_rw);
            TM_WRITE(parent_rw->m_parent, sibling_w);
            TM_WRITE(parent_rw->m_ID, cID);
            // make sibling the pID child of gparent
            TM_WRITE(gparent_w->m_child[pID], sibling_w);
            TM_WRITE(sibling_w->m_parent, gparent_w);
            TM_WRITE(sibling_w->m_ID, pID);
            // reset sibling
            sibling_w = nephew_w;
        }

        RBNode* n = TM_READ(sibling_w->m_child[1 - cID]);
        const RBNode* n_r(n); // if n is null, n_r will be null too
        if ((n != NULL) && (RED == TM_READ(n_r->m_color))) {
            // the far nephew is red
            RBNode* n_rw(n);
            /*
                  ?p          ?s
                  / \         / \
                 By  Bs  =>  Bp  Bn
                / \         / \
               ?1 Rn      By  ?1
            */
            TM_WRITE(sibling_w->m_color, TM_READ(parent_rw->m_color));
            TM_WRITE(parent_rw->m_color, BLACK);
            TM_WRITE(n_rw->m_color, BLACK);
            // promote sibling_w
            RBNode* gparent_w(TM_READ(parent_rw->m_parent));
            int pID = TM_READ(parent_rw->m_ID);
            RBNode* nephew(TM_READ(sibling_w->m_child[cID]));
            // make nephew the 1-cID child of parent
            TM_WRITE(parent_rw->m_child[1-cID], nephew);
            if (nephew != NULL) {
                RBNode* nephew_w(nephew);
                TM_WRITE(nephew_w->m_parent, parent_rw);
                TM_WRITE(nephew_w->m_ID, 1-cID);
            }
            // make parent the cID child of the sibling
            TM_WRITE(sibling_w->m_child[cID], parent_rw);
            TM_WRITE(parent_rw->m_parent, sibling_w);
            TM_WRITE(parent_rw->m_ID, cID);
            // make sibling the pID child of gparent
            TM_WRITE(gparent_w->m_child[pID], sibling_w);
            TM_WRITE(sibling_w->m_parent, gparent_w);
            TM_WRITE(sibling_w->m_ID, pID);
            break; // problem solved
        }

        n = TM_READ(sibling_w->m_child[cID]);
        n_r = n;
        if ((n != NULL) && (RED == TM_READ(n_r->m_color))) {
            /*
                    ?p          ?p
                    / \         / \
                  By  Bs  =>  By  Bn
                      / \           \
                     Rn B1          Rs
                                      \
                                      B1
            */
            RBNode* n_rw = const_cast<RBNode*>(n_r);
            TM_WRITE(sibling_w->m_color, RED);
            TM_WRITE(n_rw->m_color, BLACK);
            RBNode* t = sibling_w;
            // promote n_rw
            RBNode* gneph(TM_READ(n_rw->m_child[1-cID]));
            // make gneph the cID child of sibling
            TM_WRITE(sibling_w->m_child[cID], gneph);
            if (gneph != NULL) {
                RBNode* gneph_w(gneph);
                TM_WRITE(gneph_w->m_parent, sibling_w);
                TM_WRITE(gneph_w->m_ID, cID);
            }
            // make sibling the 1-cID child of n
            TM_WRITE(n_rw->m_child[1 - cID], sibling_w);
            TM_WRITE(sibling_w->m_parent, n_rw);
            TM_WRITE(sibling_w->m_ID, 1 - cID);
            // make n the 1-cID child of parent
            TM_WRITE(parent_rw->m_child[1 - cID], n_rw);
            TM_WRITE(n_rw->m_parent, parent_rw);
            TM_WRITE(n_rw->m_ID, 1 - cID);
            sibling_w = n_rw;
            n_rw = t;

            // now the far nephew is red... copy of code from above
            TM_WRITE(sibling_w->m_color, TM_READ(parent_rw->m_color));
            TM_WRITE(parent_rw->m_color, BLACK);
            TM_WRITE(n_rw->m_color, BLACK);
            // promote sibling_w
            RBNode* gparent_w(TM_READ(parent_rw->m_parent));
            int pID = TM_READ(parent_rw->m_ID);
            RBNode* nephew(TM_READ(sibling_w->m_child[cID]));
            // make nephew the 1-cID child of parent
            TM_WRITE(parent_rw->m_child[1-cID], nephew);
            if (nephew != NULL) {
                RBNode* nephew_w(nephew);
                TM_WRITE(nephew_w->m_parent, parent_rw);
                TM_WRITE(nephew_w->m_ID, 1-cID);
            }
            // make parent the cID child of the sibling
            TM_WRITE(sibling_w->m_child[cID], parent_rw);
            TM_WRITE(parent_rw->m_parent, sibling_w);
            TM_WRITE(parent_rw->m_ID, cID);
            // make sibling the pID child of gparent
            TM_WRITE(gparent_w->m_child[pID], sibling_w);
            TM_WRITE(sibling_w->m_parent, gparent_w);
            TM_WRITE(sibling_w->m_ID,pID);

            break; // problem solved
        }
        /*
                ?p          ?p
                / \         / \
              Bx  Bs  =>  Bp  Rs
                  / \         / \
                 B1 B2      B1  B2
        */

        TM_WRITE(sibling_w->m_color, RED); // propagate upwards

        // advance to parent and balance again
        curr = parent_rw;
    }

    // if y was red, this fixes the balance
    TM_WRITE(curr->m_color, BLACK);

    // free storage associated with deleted node
    TM_FREE(x_rw);
}


// returns black-height when balanced and -1 otherwise
int RBTree::blackHeight(const RBNode* x)
{
    if (!x)
        return 0;
    const RBNode* x_r(x);
    int bh0 = blackHeight(x_r->m_child[0]);
    int bh1 = blackHeight(x_r->m_child[1]);
    if ((bh0 >= 0) && (bh1 == bh0))
        return BLACK==x_r->m_color ? 1+bh0 : bh0;
    else
        return -1;
}

// returns true when a red node has a red child
bool RBTree::redViolation(const RBNode* p_r, const RBNode* x)
{
    if (!x)
        return false;
    const RBNode* x_r(x);
    return ((RED == p_r->m_color && RED == x_r->m_color)
            || (redViolation(x_r, x_r->m_child[0]))
            || (redViolation(x_r, x_r->m_child[1])));
}

// returns true when all nodes' parent fields point to their parents
bool RBTree::validParents(const RBNode* p, int xID, const RBNode* x)
{
    if (!x)
        return true;
    const RBNode* x_r(x);
    return ((x_r->m_parent == p)
            && (x_r->m_ID == xID)
            && (validParents(x, 0, x_r->m_child[0]))
            && (validParents(x, 1, x_r->m_child[1])));
}

// returns true when the tree is ordered
bool RBTree::inOrder(const RBNode* x, int lowerBound, int upperBound)
{
    if (!x)
        return true;
    const RBNode* x_r(x);
    return ((lowerBound <= x_r->m_val)
            && (x_r->m_val <= upperBound)
            && (inOrder(x_r->m_child[0], lowerBound, x_r->m_val - 1))
            && (inOrder(x_r->m_child[1], x_r->m_val + 1, upperBound)));
}

// build an empty tree
RBTree::RBTree() : sentinel(new RBNode()) { }

// sanity check of the RBTree data structure
bool RBTree::isSane() const
{
    const RBNode* sentinel_r(sentinel);
    RBNode* root = sentinel_r->m_child[0];

    if (!root)
        return true; // empty tree needs no checks

    const RBNode* root_r(root);
    return ((BLACK == root_r->m_color) &&
            (blackHeight(root) >= 0) &&
            !(redViolation(sentinel_r, root)) &&
            (validParents(sentinel, 0, root)) &&
            (inOrder(root, INT_MIN, INT_MAX)));
}

#endif // TREE_HPP__
