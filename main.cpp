
#include <vector>
#include <map>
#include "shm_allocator.h"
#include "shm_mm.h"

using namespace std;

using SHMmap =
map<SHMString, int, std::less<SHMString>, SHM_STL_Allocator<std::pair<const SHMString, int>>>;

class SHMRegisterInfo {
public:
    SHMRegisterInfo(
        const char *sid,
        const char *bid,
        const char *sip,
        const char *nm,
        const char *id,
        const char *lbl
        )
    :server_id(sid)
    ,board_id(bid)
    ,server_ip(sip)
    ,name(nm)
    ,id(id)
    ,label(lbl)
    ,heartbeat_key(-1)
    ,sub_key(-1)
    ,pub_key(-1)
    ,reply_key(-1)
    ,topic_key(-1)
    {}
    ~SHMRegisterInfo() = delete;

    SHMString server_id;
    SHMString board_id;
    SHMString server_ip;
    SHMString name;
    SHMString id;
    SHMString label;

    vector<SHMString, SHM_STL_Allocator<SHMString>> pub;
    vector<SHMString, SHM_STL_Allocator<SHMString>> sub;

    SHMmap channel_key;
    int heartbeat_key;
    int sub_key;
    int pub_key;
    int reply_key;
    int topic_key;
};

using SHMRegMap =
map<SHMString, SHMRegisterInfo*, std::less<SHMString>, SHM_STL_Allocator<std::pair<const SHMString, SHMRegisterInfo*>>>;

int main(int argc, char const *argv[])
{
    shm_init(0x23232323, 128);

    SHMRegMap *rm = mem_pool_attach<SHMRegMap>();
    printf("map size %lu, sizeof size_t %lu\n", rm->size(), (40+15) & (~15));

    for(auto &&i : *rm){
        printf("before server_id: %s, board_id %s, server_ip %s, name %s, id %s, label %s\n",
            i.second->server_id.c_str(), i.second->board_id.c_str(), i.second->server_ip.c_str(),
            i.second->name.c_str(), i.second->id.c_str(), i.second->label.c_str());
    }

    void *ptr = mm_malloc(sizeof(SHMRegisterInfo));
    auto *p = new(ptr)SHMRegisterInfo("a", "b", "c", "d", "e", "f");

    rm->insert({"a", p});
    for(auto &&i : *rm){
        printf("after server_id: %s, board_id %s, server_ip %s, name %s, id %s, label %s\n",
            i.second->server_id.c_str(), i.second->board_id.c_str(), i.second->server_ip.c_str(),
            i.second->name.c_str(), i.second->id.c_str(), i.second->label.c_str());
    }
    getchar();

    shm_destroy();

    return 0;
}