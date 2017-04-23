#include <algorithm>
#include <assert.h>     /* assert */
#include "MortonTree.h"
#include "log.h"

static int log_level = LOG_INFO;

MortonTree::MortonTree(RectangleD bound) {
    this->bound = bound;
}

MortonTree::~MortonTree() {
    for (int i = 0; i < this->cells.size(); i++) {
        MortonCell *cell = this->cells[i];
        if (cell != NULL) {
            delete cell;
        }
    }
}

// fill in MortonTreeObject vector using the specified objects
void 
MortonTree::fillMortonTreeObjects(std::vector<Object> &objects) {
    ENTER();
    int n = objects.size();
    for (int i = 0; i < n; i++) {
        Object o = objects[i];
        this->objects.push_back(MortonTreeObject(o.position, o.speed, o.mass, this->bound));
    }
    std::sort(this->objects.begin(), this->objects.end(), MortonTreeObjectEval());
    LEAVE();
}

// generate the whole MortonTree, until all the cells become leafs or the max depth of the tree reaches
void 
MortonTree::generateMortonTree() {
    ENTER();
    if (objects.size() == 0) {
        ERR("No objects available\n");
        return;
    }

    std::vector<CellInfo> old_info;
    std::vector<CellInfo> new_info;
    MortonCell *cell = new MortonCell();
    this->cells.push_back(cell);
    old_info.push_back(CellInfo(1, 0, this->objects.size(), 0)); // root is at level 0

    while (old_info.size() > 0) {
        generateMortonCell(old_info, new_info);
        //traverseObjects();
        new_info.swap(old_info);
        new_info.clear();
    }
    LEAVE();
}

// generate the MortonCell based on the tree leve, and start, size of cells
void
MortonTree::generateMortonCell(std::vector<CellInfo> &old_info, std::vector<CellInfo> &new_info) {
    //ENTER();
    for (int i = 0; i < old_info.size(); i++) {
        int level = old_info[i].level;
        int start = old_info[i].start;
        int size = old_info[i].size;
        int parent = old_info[i].parent;
        int end = start + size;

        if (!isValidObjectsIndex(start, size)) {
            ERR("Invalid index, %d, %d\n", start, size);
            return;
        }
        
        unsigned int level_mask = get_level_mask(level);
        MortonCell *cell = new MortonCell();
        MortonTreeObject *o = &this->objects[start];
        unsigned int old_mask = o->mcode & level_mask;
        cell->bound = rectangled_incr_bound(o->position);
        cell->com = Object::add(cell->com, *o);
        o->parent = this->cells.size();  // the index of cell for this object
        int first_index = start;
        int count = 1;

        for (int j = start + 1; j < end; j++) {
            MortonTreeObject *o = &this->objects[j];
            unsigned int new_mask = o->mcode & level_mask;

            if (new_mask == old_mask) {
                count++;
                cell->bound = rectangled_incr_bound(cell->bound, o->position);
                cell->com = Object::add(cell->com, *o);
                o->parent = this->cells.size();  // the index of cell for this object
                //printf("\e[1;34mobj: %d, parent: %d\e[0m\n", j, o->parent);
            } else {
                // if the group size of these objects is not greater than THRES_OBJS, then regard the cell as a leaf
                if (count <= THRES_OBJS) {
                    cell->is_leaf = true;
                } else {
                    cell->is_leaf = false;
                    // the index of the newest cell is the parent
                    new_info.push_back(CellInfo(level + 1, first_index, count, this->cells.size() + 1));
                }
                cell->first_index = first_index;
                cell->count = count;
                cell->parent = parent;
                this->cells.push_back(cell);
                // updat the children list for the new cell
                this->cells[parent]->children.push_back(this->cells.size() - 1);

                // create a new cell and update related info
                cell = new MortonCell();
                cell->bound = rectangled_incr_bound(o->position);
                cell->com = Object::add(cell->com, *o);
                o->parent = this->cells.size();  // the index of cell for this object
                first_index = j;
                count = 1;
            }
            old_mask = new_mask;
        }

        // update info for the last new cell
        if (count <= THRES_OBJS) {
            cell->is_leaf = true; 
        } else {
            cell->is_leaf = false;
            new_info.push_back(CellInfo(level + 1, first_index, count, this->cells.size() + 1));
        }
        cell->first_index = first_index;
        cell->count = count;
        cell->parent = parent;
        this->cells.push_back(cell);
        this->cells[parent]->children.push_back(this->cells.size() - 1);
    }
    //LEAVE();
}

// calculate the total force exerted on an object specified with the index
Point2D 
MortonTree::getForceOnObject(int obj_idx) {
    ENTER();
    
    GS_FLOAT s, d;
    Point2D dr, result = point2d_zero();
    MortonTreeObject tar_obj = this->objects[obj_idx];
    int i = 0;
    int end = this->objects.size();

    while (i < end) {
        // every loop check a leaf cell
        MortonTreeObject curr_obj = this->objects[i];
        int curr_cell_idx = curr_obj.parent;
        assert(curr_cell_idx >= 0);
        MortonCell *curr_cell = this->cells[curr_cell_idx];
        s = MAX(curr_cell->bound.size.x, curr_cell->bound.size.y);
        DBG("curr_cell_idx:%d, x:%f, y:%f\n", curr_cell_idx, curr_cell->bound.size.x, curr_cell->bound.size.y);
        dr = point2d_sub(tar_obj.position, curr_cell->com.position);
        d = point2d_length(dr);

        // FIXME only try the leaf layer, maybe can go upstairs further
        if ((s/d) < SD_TRESHOLD) {
            result = point2d_add(result, Object::calculate_force(tar_obj, curr_cell->com));
        } else {
            for (int j = i; j < curr_cell->count; j++) {
                if (j == obj_idx) {
                    continue;
                }
                MortonTreeObject curr_obj = this->objects[j];
                result = point2d_add(result, Object::calculate_force(tar_obj, curr_obj));
            } 
        }
        i += curr_cell->count;
    }

    LEAVE();
    return result;
}

void
MortonTree::traverseCells() {
    ENTER();
    for (int i = 0; i < this->cells.size(); i++) {
        MortonCell *cell = this->cells[i];
        printf("cell:%d, is_leaf:%d, start:%d, size:%d, parent:%d\n", i, cell->is_leaf, cell->first_index, cell->count, cell->parent);
        printf("children: ");
        for (int j = 0; j < cell->children.size(); j++) {
            printf("%d, ", cell->children[j]);
        }
        printf("\n");
    }
    LEAVE();
}

void
MortonTree::traverseObjects() {
    ENTER();
    for (int i = 0; i < this->objects.size(); i++) {
        MortonTreeObject o = this->objects[i];
        printf("obj: %d, parent:%d, mcode:0x%u\n", i, o.parent, o.mcode);
    }   
    LEAVE();
}

// update all the objects' speed
void 
MortonTree::applyToObjects(GS_FLOAT dt) {
    ENTER();
    MortonTreeObject *obj;

    for (int i = 0; i < this->objects.size(); i++) {
        Point2D acc = getForceOnObject(i);
        Point2D dv = point2d_multiply(acc, dt);
        obj = &this->objects[i];
        obj->speed = point2d_add(obj->speed, dv);
    }
    LEAVE();
}

bool
MortonTree::isValidObjectsIndex(int start, int size) {
    int n = this->objects.size();
    //DBG("n: %d, start: %d, size: %d\n", n, start, size);
    return (n > 0) && (start >= 0) && (size >= 0) 
        && (start + size <= n); 
}
