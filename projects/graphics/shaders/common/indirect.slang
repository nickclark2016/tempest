// indirect.slang

struct DrawIndexedIndirectCommand {
    uint index_count;
    uint instance_count;
    uint first_index;
    int vertex_offset;
    uint first_instance;
};