#include <ultra64.h>
#include "game/model.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "native_test_assert.h"

int main(int argc, char **argv) {
    assert(argc == 2);
    FILE *file = fopen(argv[1], "rb");
    assert(file);
    unsigned char record[28];
    unsigned nodes = 0, checks = 0, both_orders = 0;
    while (fread(record, 1, sizeof record, file) == sizeof record) {
        union ModelRoData data = {0}, group_data = {0};
        ModelNode parent = {0}, node = {0}, left = {0}, right = {0};
        Model model = {0};
        RenderPosView transform = {0};
        u32 rw[1] = {0};
        memcpy(data.BSP.Point.f, record, 12);
        memcpy(data.BSP.Vector.f, record + 12, 12);
        memcpy(&data.BSP.reserved, record + 24, 2);
        assert(data.BSP.reserved >= 0 && data.BSP.reserved <= 3);
        data.BSP.leftChild = &left;
        data.BSP.rightChild = &right;
        node.Opcode = MODELNODE_OPCODE_BSP;
        node.Data = &data;
        node.Parent = &parent;
        parent.Opcode = MODELNODE_OPCODE_GROUP;
        parent.Data = &group_data;
        model.datas = rw;
        model.render_pos = &transform;
        unsigned orders = 0;
        // Rotate and translate the actual retail planes across the eye. Check
        // the real update function AND its resulting linked draw-order lists.
        for (unsigned angle = 0; angle < 8; ++angle) {
            float a = angle * .61f;
            memset(&transform, 0, sizeof transform);
            Mtxf *m = &transform.pos;
            m->m[0][0] = m->m[2][2] = cosf(a);
            m->m[0][2] = sinf(a);
            m->m[2][0] = -sinf(a);
            m->m[1][1] = m->m[3][3] = 1;
            double normal[3] = {0}, point[3] = {0};
            for (unsigned j = 0; j < 3; ++j)
                for (unsigned i = 0; i < 3; ++i) {
                    if (!data.BSP.reserved || i == (unsigned)data.BSP.reserved - 1)
                        normal[j] += m->m[i][j] * (double)data.BSP.Vector.f[i];
                    point[j] += m->m[i][j] * (double)data.BSP.Point.f[i];
                }
            for (int side = -1; side <= 1; side += 2) {
                double dot = 0;
                for (unsigned j = 0; j < 3; ++j) {
                    m->m[3][j] = (float)(-point[j] + side * normal[j] * 100);
                    dot += normal[j] * (point[j] + m->m[3][j]);
                }
                if (fabs(dot) < .00001)
                    continue;
                int visible = dot < 0;
                modelUpdateReorderRelations(&model, &node);
                assert((rw[0] != 0) == visible);
                ModelNode *first = visible ? &left : &right;
                ModelNode *last = visible ? &right : &left;
                assert(node.Child == first && first->Prev == NULL);
                assert(first->Next == last && last->Prev == first && last->Next == NULL);
                orders |= 1u << visible;
                checks++;
            }
        }
        both_orders += orders == 3;
        nodes++;
    }
    assert(feof(file) && nodes && checks && both_orders == nodes);
    fclose(file);
    printf("PASS original BSP sorter: %u retail planes, %u rotated/translated poses, both draw orders and sibling links\n", nodes, checks);
}
