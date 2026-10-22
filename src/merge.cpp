// merge.cpp - 多设备同步冲突合并（按事件 updated 时间取较新）
#include "app.h"
#include <algorithm>
#include <map>

namespace {

std::wstring EventUpdatedKey(const Event& e) {
    // 优先 updated，其次 created，都没有返回空（按本地优先）
    if (e.updated.y != 0) return ISODT(e.updated);
    if (e.created.y != 0) return ISODT(e.created);
    return L"";
}

bool RemoteNewer(const Event& local, const Event& remote) {
    std::wstring lk = EventUpdatedKey(local);
    std::wstring rk = EventUpdatedKey(remote);
    if (rk.empty()) return false;
    if (lk.empty()) return true;
    return CompareDT(remote.updated.y ? remote.updated : remote.created,
                     local.updated.y ? local.updated : local.created) > 0;
}

}  // namespace

Store MergeStores(const Store& local, const Store& remote, MergeStats& st) {
    st = MergeStats{};
    Store out = local;   // 以本地为基底：设置/偏好/学期等本地优先

    // 墓碑合并：两边并集，同 id 取较新删除时间
    std::map<std::string, std::wstring> tombMap;
    for (auto& t : local.tombstones) tombMap[t.first] = t.second;
    for (auto& t : remote.tombstones) {
        auto it = tombMap.find(t.first);
        if (it == tombMap.end() || CompareDT(ParseISODT(t.second, nullptr), ParseISODT(it->second, nullptr)) > 0)
            tombMap[t.first] = t.second;
    }
    out.tombstones.clear();
    for (auto& kv : tombMap) out.tombstones.push_back({ kv.first, kv.second });
    while (out.tombstones.size() > 300) out.tombstones.erase(out.tombstones.begin());

    // 事件按 id 合并
    std::map<std::string, const Event*> localById;
    for (auto& e : local.events) localById[e.id] = &e;
    std::map<std::string, const Event*> remoteById;
    for (auto& e : remote.events) remoteById[e.id] = &e;

    std::vector<Event> merged;
    for (auto& e : local.events) {
        auto it = remoteById.find(e.id);
        if (it == remoteById.end()) {
            merged.push_back(e);
            continue;
        }
        if (RemoteNewer(e, *it->second)) {
            merged.push_back(*it->second);
            ++st.updated;
        } else {
            merged.push_back(e);
            ++st.kept;
        }
    }
    for (auto& e : remote.events) {
        if (localById.count(e.id)) continue;
        merged.push_back(e);
        ++st.added;
    }
    st.localOnly = (int)local.events.size() - st.kept - st.updated;
    st.remoteOnly = st.added;
    out.events = std::move(merged);

    // 标签按 id 合并：本地优先，缺失追加
    std::map<std::string, bool> localTags;
    for (auto& t : out.tags) localTags[t.id] = true;
    for (auto& t : remote.tags) {
        if (localTags.count(t.id)) continue;
        out.tags.push_back(t);
        ++st.tagsAdded;
    }

    // 喝水记录：本地没有的日期从服务器补
    for (auto& kv : remote.water) {
        if (!out.water.count(kv.first)) out.water[kv.first] = kv.second;
    }

    // 应用墓碑：删除时间晚于事件修改时间（或事件无时间戳）的事件
    {
        std::vector<Event> kept;
        for (auto& e : out.events) {
            std::wstring delAt;
            for (auto& t : out.tombstones) {
                if (t.first == e.id) { delAt = t.second; break; }
            }
            if (!delAt.empty()) {
                DateTime tomb = ParseISODT(delAt, nullptr);
                DateTime evT = (e.updated.y != 0) ? e.updated : e.created;
                bool evNewer = (evT.y != 0) && CompareDT(evT, tomb) > 0;
                if (!evNewer) {
                    ++st.deleted;
                    continue;
                }
            }
            kept.push_back(e);
        }
        out.events = std::move(kept);
    }

    out.updatedAt = local.updatedAt;
    return out;
}
