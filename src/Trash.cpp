#include "Trash.hpp"
#include <hjfod.gmd-api/include/GMD.hpp>

using namespace geode::prelude;
using namespace matjson;
using namespace asp;

static const std::string METADATA_FILE = "metadata.json";
static const std::string TRASH_ITEMS_KEY = "trash-times";

static std::string convertToKebabCase(const std::string_view string) {
    std::string result;
    char last = '\0';

    for (char c : string) {
        // Add a dash if the character is in uppercase (camelCase / PascalCase)
        // or a space (Normal case) or an underscore (snake_case) and the
        // built result string isn't empty and make sure there's only a
        // singular dash
        // Don't add a dash if the previous character was also uppercase or a
        // number (SCREAM1NG L33TCASE should be just scream1ng-l33tcase)
        if (result.size() && result.back() != '-' && (
            (std::isupper(c) && !(std::isupper(last) || std::isdigit(last))) ||
            std::isspace(c) ||
            c == '_'
        )) {
            result.push_back('-');
        }

        // Only preserve alphanumeric characters
        if (std::isalnum(c)) {
            result.push_back(std::tolower(c));
        }

        last = c;
    }

    // If there is a dash at the end (for example because the name ended in a
    // space) then get rid of that
    if (result.back() == '-') {
        result.pop_back();
    }

    return result;
}

static std::string correctReservedFilenames(std::string name) {
    switch (hash(name.c_str())) {
        case hash("con"): case hash("prn"): case hash("aux"): case hash("nul"):
        // This was in https://www.boost.org/doc/libs/1_36_0/libs/filesystem/doc/portability_guide.htm?
        // Never heard of it before though
        case hash("clock$"):
        case hash("com1"): case hash("com2"): case hash("com3"): case hash("com4"):
        case hash("com5"): case hash("com6"): case hash("com7"): case hash("com8"): case hash("com9"):
        case hash("lpt1"): case hash("lpt2"): case hash("lpt3"): case hash("lpt4"):
        case hash("lpt5"): case hash("lpt6"): case hash("lpt7"): case hash("lpt8"): case hash("lpt9"): {
            name += "-0";
        } break;
    }

    return name;
}

// Recover some of the old formats like BetterSave n stuff
static void recoverOldTrashcan(const std::filesystem::path& dirToRecover, size_t& succeeded, size_t& failed) {
    const std::filesystem::path trashDir = Trashcan::get()->getTrashDir();

    for (std::filesystem::path dir : file::readDirectory(dirToRecover).unwrapOrDefault()) {
        std::error_code error;

        if (std::filesystem::exists(dir / "level.gmd")) {
            std::filesystem::rename(dir / "level.gmd", trashDir / (dir.filename().string() + ".gmd"), error);

            if (error) {
                failed += 1;
                log::error("Failed to recover trashed level: {}", error.message());
            } else {
                succeeded += 1;
            }
        } else if (std::filesystem::exists(dir / "list.gmdl")) {
            std::filesystem::rename(dir / "list.gmdl", trashDir / (dir.filename().string() + ".gmdl"), error);

            if (error) {
                failed += 1;
                log::error("Failed to recover trashed list: {}", error.message());
            } else {
                succeeded += 1;
            }
        }
    }
}

TrashedItem::TrashedItem(LevelOrList levelOrList, std::filesystem::path path, SystemTime time):
m_levelOrList(std::move(levelOrList)),
m_path(std::move(path)),
m_trashTime(std::move(time)) { }

std::string TrashedItem::getName() const {
    return std::visit(makeVisitor {
        [](GJGameLevel* level) { return level->m_levelName; },
        [](GJLevelList* list) { return list->m_listName; }
    }, m_levelOrList);
}

SystemTime TrashedItem::getTrashTime() const {
    return m_trashTime;
}

bool TrashedItem::isList() const {
    return std::holds_alternative<Ref<GJLevelList>>(m_levelOrList);
}

Trashcan* Trashcan::get() {
    static Trashcan* INSTANCE = new Trashcan();

    return INSTANCE;
}

Result<> Trashcan::saveMetadata() {
    Value trashTimes = matjson::Value::object();

    for (std::shared_ptr<TrashedItem> item : m_items) {
        trashTimes[string::pathToString(item->m_path.filename())] = item->m_trashTime.timeSinceEpoch().seconds();
    }

    return file::writeString(this->getTrashDir() / METADATA_FILE, matjson::makeObject({
        { TRASH_ITEMS_KEY, std::move(trashTimes) }
    }).dump());
}

Result<> Trashcan::loadItem(const std::filesystem::path& path, std::optional<SystemTime> knownTime) {
    // Load time, using last file write time as fallback
    std::error_code error;
    SystemTime time = knownTime ? std::move(knownTime).value() : SystemTime::fromUnix(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::filesystem::last_write_time(path, error).time_since_epoch()
            // I will personally serve Bill Gates a really lousy plate of spaghetti
            GEODE_WINDOWS(- std::chrono::years(369))
        ).count()
    );

    if (path.filename() == METADATA_FILE) {
        return Ok();
    } else if (path.extension() == ".gmd") {
        if (geode::Result<GJGameLevel*> level = gmd::importGmdAsLevel(path)) {
            m_items.push_back(std::make_shared<TrashedItem>(std::move(level).unwrap(), std::move(path), std::move(time)));
        } else {
            return Err("Unable to read trashed level {} from metadata: {}", std::move(path), std::move(level).unwrapErr());
        }
    } else if (path.extension() == ".gmdl") {
        if (geode::Result<geode::Ref<GJLevelList>> list = gmd::importGmdAsList(path)) {
            m_items.push_back(std::make_shared<TrashedItem>(std::move(list).unwrap(), std::move(path), std::move(time)));
        } else {
            return Err("Unable to read trashed list {} from metadata: {}", std::move(path), std::move(list).unwrapErr());
        }
    } else {
        return Err("Unknown file type in trashed level {} from metadata", std::move(path));
    }

    return Ok();
}

bool Trashcan::isLoaded() const {
    return m_loaded;
}

std::filesystem::path Trashcan::getTrashDir() const {
    std::filesystem::path path = dirs::getSaveDir() / "trashed-levels";

    if (!std::filesystem::is_directory(path)) {
        if (Result<> result = file::createDirectoryAll(path); result.isErr()) {
            log::error("{}", std::move(result).unwrapErr());
        }
    }

    return path;
}

std::string Trashcan::getFreeID(const std::string_view original, const std::string_view extension) {
    const std::filesystem::path dir = this->getTrashDir();
    // Synthesize an ID for the level by taking the level name in kebab-case
    // and then adding an incrementing number at the end until there exists
    // no folder with the same name already
    std::string name = convertToKebabCase(original);

    // Prevent names that are too long (some people might use input bypass
    // to give levels absurdly long names)
    if (name.size() > 20) {
        name = name.substr(0, 20);
    } else if (name.empty()) {
        name = "unnamed";
    }

    name = correctReservedFilenames(std::move(name));
    std::string id = fmt::format("{}.{}", name, extension);

    for (size_t i = 0; std::filesystem::exists(dir / id); i++) {
        id = fmt::format("{}-{}.{}", name, i, extension);
    }

    return id;
}

const std::vector<std::shared_ptr<TrashedItem>>& Trashcan::getItems() const {
    return m_items;
}

Result<> Trashcan::load() {
    std::unordered_set<std::string> succesfullyLoadedWithMetadata;
    size_t trashedItems = 0;
    size_t trashedFailed = 0;
    m_loaded = true;

    log::debug("Recovering old trashcan mod files...");

    recoverOldTrashcan(dirs::getSaveDir() / "levels" / "trashcan", trashedItems, trashedFailed);
    recoverOldTrashcan(dirs::getSaveDir() / "bettersave.trash", trashedItems, trashedFailed);

    log::debug("Recovered {} trashcan items ({} failed)", trashedItems, trashedFailed);

    for (std::shared_ptr<TrashedItem> item : m_items) {
        succesfullyLoadedWithMetadata.emplace(item->m_path.filename().string());
    }

    // First load levels by going through metadata.json and loading everything
    // listed there (new proper format)
    const std::filesystem::path metadataPath = this->getTrashDir() / METADATA_FILE;
    log::debug("Loading trashed levels");

    if (!std::filesystem::exists(metadataPath)) {
        return this->saveMetadata();
    }

    const Value metadata = GEODE_UNWRAP(file::readJson(metadataPath));

    for (auto&& [key, time] : GEODE_UNWRAP(metadata.get(TRASH_ITEMS_KEY))) {
        if (succesfullyLoadedWithMetadata.contains(key)) {
            log::warn("File {} was included multiple times in trash metadata? That's odd...", key);
            continue;
        }

        Result<> load = this->loadItem(this->getTrashDir() / key, time.asUInt().mapOrElse(
            [] { return std::nullopt; },
            [](const std::uintmax_t timestamp) { return std::optional(SystemTime::fromUnix(timestamp)); }
        ));

        if (load.isOk()) {
            succesfullyLoadedWithMetadata.emplace(std::move(key));
        } else {
            log::error("{}", std::move(load).unwrapErr());
        }
    }

    // Then fallback check for any levels not covered by metadata.json
    for (std::filesystem::path path : file::readDirectory(this->getTrashDir()).unwrapOrDefault()) {
        if (!succesfullyLoadedWithMetadata.contains(string::pathToString(path.filename()))) {
            if (Result<> result = this->loadItem(path, std::nullopt); result.isErr()) {
                log::warn("{}", std::move(result).unwrapErr());
            }
        }
    }

    // Sort items by trash time
    std::sort(m_items.begin(), m_items.end(), [](std::shared_ptr<TrashedItem> a, std::shared_ptr<TrashedItem> b) {
        return a->m_trashTime > b->m_trashTime;
    });

    // Save any new levels we found
    return this->saveMetadata();
}

Result<> Trashcan::trash(GJGameLevel* level) {
    const std::filesystem::path path = this->getTrashDir() / this->getFreeID(level->m_levelName, "gmd");

    if (Result<> result = gmd::exportLevelAsGmd(level, path); result.isErr()) {
        return Err(std::move(result).unwrapErr());
    }

    const std::shared_ptr<TrashedItem> item = std::make_shared<TrashedItem>(level, path, SystemTime::now());

    m_items.insert(m_items.begin(), item);
    LocalLevelManager::get()->m_localLevels->removeObject(level);
    UpdateTrashEvent().send();

    return this->saveMetadata();
}

Result<> Trashcan::trash(GJLevelList* list) {
    const std::filesystem::path path = this->getTrashDir() / getFreeID(list->m_listName, "gmdl");

    if (Result<> save = gmd::exportListAsGmd(list, path); save.isErr()) {
        return Err(std::move(save).unwrapErr());
    }

    const std::shared_ptr<TrashedItem> item = std::make_shared<TrashedItem>(list, path, SystemTime::now());

    m_items.insert(m_items.begin(), item);
    LocalLevelManager::get()->m_localLists->removeObject(list);
    UpdateTrashEvent().send();

    return this->saveMetadata();
}

Result<> Trashcan::untrash(std::shared_ptr<TrashedItem> item) {
    std::visit(makeVisitor {
        [](GJGameLevel* level) { LocalLevelManager::get()->m_localLevels->insertObject(level, 0); },
        [](GJLevelList* list) { LocalLevelManager::get()->m_localLists->insertObject(list, 0); }
    }, item->m_levelOrList);

    std::error_code error;
    std::filesystem::remove(item->m_path, error);

    if (error) {
        return Err("Unable to delete trashed file: {} (code {})", error.message(), error.value());
    }

    std::erase(m_items, item);
    UpdateTrashEvent().send();

    return this->saveMetadata();
}

Result<> Trashcan::deletePermanently(std::shared_ptr<TrashedItem> item) {
    std::error_code error;
    std::filesystem::remove(item->m_path, error);

    if (error) {
        return Err("Unable to delete trashed file: {} (code {})", error.message(), error.value());
    }

    std::erase(m_items, item);
    UpdateTrashEvent().send();

    return this->saveMetadata();
}

Result<> Trashcan::deleteAllPermanently() {
    std::error_code error;
    std::filesystem::remove_all(this->getTrashDir(), error);

    if (error) {
        return Err("Unable to clear trashcan: {} (code {})", error.message(), error.value());
    }

    m_items.clear();
    UpdateTrashEvent().send();

    return this->saveMetadata();
}
