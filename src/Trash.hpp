#pragma once

#include <Geode/Geode.hpp>

class UpdateTrashEvent : public geode::Event<UpdateTrashEvent, bool()> { };

class Trashcan;

class TrashedItem final {
    using LevelOrList = std::variant<geode::Ref<GJGameLevel>, geode::Ref<GJLevelList>>;

    LevelOrList m_levelOrList;
    std::filesystem::path m_path;
    asp::SystemTime m_trashTime;

    friend class Trashcan;
public:
    TrashedItem(LevelOrList levelOrList, std::filesystem::path path, asp::SystemTime time);

    std::string getName() const;
    asp::SystemTime getTrashTime() const;
    bool isList() const;
};

class Trashcan final {
public:
    static Trashcan* get();
private:
    std::vector<std::shared_ptr<TrashedItem>> m_items;
    bool m_loaded = false;

    geode::Result<> saveMetadata();
    geode::Result<> loadItem(const std::filesystem::path& path, std::optional<asp::SystemTime> time);
public:
    bool isLoaded() const;

    std::filesystem::path getTrashDir() const;
    std::string getFreeID(const std::string_view original, const std::string_view extension);

    const std::vector<std::shared_ptr<TrashedItem>>& getItems() const;

    geode::Result<> load();

    geode::Result<> trash(GJGameLevel* level);
    geode::Result<> trash(GJLevelList* list);

    geode::Result<> untrash(std::shared_ptr<TrashedItem> item);
    geode::Result<> deletePermanently(std::shared_ptr<TrashedItem> item);
    geode::Result<> deleteAllPermanently();
};
