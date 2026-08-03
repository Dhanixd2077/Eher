#include <Geode/modify/GameLevelManager.hpp>
#include <Geode/modify/LevelBrowserLayer.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include "Trash.hpp"
#include "TrashcanPopup.hpp"

using namespace geode::prelude;

struct $modify(GameLevelManager) {
    $override void deleteLevel(GJGameLevel* level) {
        if (level->m_levelType == GJLevelType::Editor) {
            if (Result<> result = Trashcan::get()->trash(level); result.isErr()) {
                FLAlertLayer::create(
                    "Error Trashing Level",
                    fmt::format("Unable to move level to trash: {}", std::move(result).unwrapErr()),
                    "OK"
                )->show();
            }

            return;
        }

        GameLevelManager::deleteLevel(level);
    }

    $override void deleteLevelList(GJLevelList* list) {
        if (list->m_listType == GJLevelType::Editor) {
            if (Result<> result = Trashcan::get()->trash(list); result.isErr()) {
                FLAlertLayer::create(
                    "Error Trashing List",
                    fmt::format("Unable to move list to trash: {}", std::move(result).unwrapErr()),
                    "OK"
                )->show();
            }

            return;
        }

        GameLevelManager::deleteLevelList(list);
    }
};

class $modify(TrashBrowserLayer, LevelBrowserLayer) {
    struct Fields {
        ListenerHandle listener;
    };

    $override bool init(GJSearchObject* search) {
        if (!LevelBrowserLayer::init(search)) return false;
        if (search->m_searchType != SearchType::MyLevels) return true;

        if (CCNode* menu = this->getChildByID("my-levels-menu")) {
            CCSprite* trashSpr = CCSprite::createWithSpriteFrameName("GJ_trashBtn_001.png");
            CCMenuItemSpriteExtra* trashBtn = CCMenuItemSpriteExtra::create(trashSpr, this, menu_selector(TrashBrowserLayer::onTrashcan));

            menu->addChild(trashBtn);
            menu->updateLayout();

            m_fields->listener = UpdateTrashEvent().listen([this, trashSpr] {
                const bool finnsTrashed = !Trashcan::get()->isLoaded() || !Trashcan::get()->getItems().empty();

                trashSpr->setOpacity(finnsTrashed ? 255 : 205);
                trashSpr->setColor(finnsTrashed ? ccWHITE : ccc3(90, 90, 90));
                // Reload the page
                this->loadPage(m_searchObject);
            });

            // Immediately load
            UpdateTrashEvent().send();
        }

        return true;
    }

    $override void onDeleteSelected(CCObject* sender) {
        if (m_searchObject->m_searchType != SearchType::MyLevels) return LevelBrowserLayer::onDeleteSelected(sender);

        size_t count = 0;

        for (auto level : CCArrayExt<GJGameLevel*>(m_levels)) {
            if (level->m_selected) {
                count += 1;
            }
        }

        if (count > 0) {
            FLAlertLayer* alert = FLAlertLayer::create(
                this,
                "Trash levels",
                fmt::format(
                    "Are you sure you want to <cr>trash</c> the <cp>{0}</c> selected level{1}?\n"
                    "<cy>You can restore the level{1} or permanently delete {2} through the Trashcan.</c>",
                    count, (count == 1 ? "" : "s"), (count == 1 ? "it" : "them")
                ),
                "Cancel", "Trash",
                340
            );

            alert->setTag(5);
            alert->m_button2->updateBGImage("GJ_button_06.png");
            alert->show();
        } else {
            FLAlertLayer::create("Nothing here...", "No levels selected.", "OK")->show();
        }
    }

    void onTrashcan(CCObject*) {
        if (!Trashcan::get()->isLoaded()) {
            if (Result<> result = Trashcan::get()->load(); result.isErr()) {
                log::warn("{}", result.unwrapErr());

                Notification::create(
                    fmt::format("Failed to load the trashcan.\n{}", std::move(result).unwrapErr()),
                    NotificationIcon::None,
                    0.5f
                )->show();
            } else {
                this->onTrashcan(nullptr);
            }
        } else if (Trashcan::get()->getItems().empty()) {
            FLAlertLayer::create(
                "Trash is Empty",
                "You have not <co>trashed</c> any levels!",
                "OK"
            )->show();
        } else {
            TrashcanPopup::create()->show();
        }
    }
};

class $modify(EditLevelLayer) {
    $override void confirmDelete(CCObject*) {
        FLAlertLayer* alert = FLAlertLayer::create(
            this,
            "Trash level",
            "Are you sure you want to <cr>trash</c> this level?\n"
            "<cy>You can restore the level or permanently delete it through the Trashcan.</c>",
            "Cancel", "Trash",
            340
        );

        alert->setTag(4);
        alert->m_button2->updateBGImage("GJ_button_06.png");
        alert->show();
    }
};