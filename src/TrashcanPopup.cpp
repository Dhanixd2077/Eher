#include "TrashcanPopup.hpp"

using namespace geode::prelude;
using namespace asp;

static std::string toAgoString(SystemTime const& time) {
    auto const fmtPlural = [](auto count, auto unit) {
        if (count == 1) {
            return fmt::format("{} {} ago", count, unit);
        } else {
            return fmt::format("{} {}s ago", count, unit);
        }
    };
    std::optional<Duration> dur = SystemTime::now().durationSince(time);

    if (dur) {
        if (dur->seconds() < 60) {
            return "Just now";
        } else if (dur->minutes() < 60) {
            return fmtPlural(dur->minutes(), "minute");
        } else if (dur->hours() < 24) {
            return fmtPlural(dur->hours(), "hour");
        } else if (dur->days() < 31) {
            return fmtPlural(dur->days(), "day");
        }
    }

    return time.format("{:%b %d %Y}");
}

class TrashedItemNode : public CCNode {
public:
    static TrashedItemNode* create(std::shared_ptr<TrashedItem> item) {
        TrashedItemNode* trashedItem = new TrashedItemNode();

        if (trashedItem && trashedItem->init(item)) {
            trashedItem->autorelease();

            return trashedItem;
        } else {
            delete trashedItem;

            return nullptr;
        }
    }
private:
    std::shared_ptr<TrashedItem> m_item;

    bool init(std::shared_ptr<TrashedItem> item) {
        if (!CCNode::init()) return false;

        constexpr float SIZE_MULTIPLIER = 1.25f;
        NineSlice* bg = NineSlice::create("square02b_001.png");
        CCLabelBMFont* title = CCLabelBMFont::create(item->getName().c_str(), "bigFont.fnt");
        CCSprite* timeIcon = CCSprite::createWithSpriteFrameName("GJ_timeIcon_001.png");
        CCLabelBMFont* time = CCLabelBMFont::create(fmt::format("{}", toAgoString(item->getTrashTime())).c_str(), "goldFont.fnt");
        CCMenu* menu = CCMenu::create();
        CCSprite* restoreSpr = CCSprite::createWithSpriteFrameName("GJ_undoBtn_001.png");

        restoreSpr->setScale(0.5f * SIZE_MULTIPLIER);

        CCMenuItemSpriteExtra* restoreBtn = CCMenuItemSpriteExtra::create(restoreSpr, this, menu_selector(TrashedItemNode::onRestore));
        CCSprite* permDelSpr = CCSprite::createWithSpriteFrameName("GJ_trashBtn_001.png");

        permDelSpr->setScale(0.4f * SIZE_MULTIPLIER);

        CCMenuItemSpriteExtra* permDelBtn = CCMenuItemSpriteExtra::create(permDelSpr, this, menu_selector(TrashedItemNode::onPermaDelete));
        m_item = item;

        this->setContentSize(ccp(300, 30 * SIZE_MULTIPLIER));

        bg->setColor(ccBLACK);
        bg->setOpacity(90);
        bg->setScale(0.5f);
        bg->setContentSize(m_obContentSize / bg->getScale());
        this->addChildAtPosition(bg, Anchor::Center);

        title->setScale(0.35f * SIZE_MULTIPLIER);

        if (item->isList()) {
            title->setColor({ 0, 255, 0 });
        }

        this->addChildAtPosition(title, Anchor::Left, ccp(5, 7) * SIZE_MULTIPLIER, ccp(0, 0.5f));

        timeIcon->setScale(0.45f * SIZE_MULTIPLIER);
        this->addChildAtPosition(timeIcon, Anchor::Left, ccp(5, -7) * SIZE_MULTIPLIER, ccp(0, 0.5f));

        time->setScale(0.35f * SIZE_MULTIPLIER);
        this->addChildAtPosition(time, Anchor::Left, ccp(20, -7) * SIZE_MULTIPLIER, ccp(0, 0.5f));

        menu->ignoreAnchorPointForPosition(false);
        menu->setContentWidth(100);
        menu->addChild(restoreBtn);
        menu->addChild(permDelBtn);
        menu->setLayout(SimpleRowLayout::create()
            ->setMainAxisAlignment(MainAxisAlignment::End)
            ->setGap(5));
        this->addChildAtPosition(menu, Anchor::Right, ccp(-5, 0) * SIZE_MULTIPLIER, ccp(1, 0.5f));

        return true;
    }

    void onRestore(CCObject*) {
        createQuickPopup(
            "Delete Permanently",
            fmt::format(
                "Do you want to <cj>restore</c> <cy>{}</c>?\n"
                "This will return it to the top of your created levels list.",
                m_item->getName()
            ),
            "Cancel", "Restore",
            [item = m_item](FLAlertLayer*, const bool btn2) {
                if (!btn2) return;

                if (Result<> result = Trashcan::get()->untrash(item); result.isOk()) {
                    Notification::create(fmt::format("Restored {}", item->getName()), NotificationIcon::Success)->show();
                } else {
                    FLAlertLayer::create("Unable to Restore", result.unwrapErr(), "OK");
                }
            }
        );
    }

    void onPermaDelete(CCObject*) {
        createQuickPopup(
            "Delete Permanently",
            fmt::format(
                "Are you SURE you want to <cr>permanently delete</c> <cy>{}</c>?\n"
                "<co>THIS ACTION IS IRREVERSIBLE!</c>",
                m_item->getName()
            ),
            "Cancel", "Delete",
            [item = m_item](FLAlertLayer*, const bool btn2) {
                if (!btn2) return;

                if (Result<> result = Trashcan::get()->deletePermanently(item); result.isOk()) {
                    Notification::create(fmt::format("Deleted {}", item->getName()))->show();
                } else {
                    FLAlertLayer::create("Unable to Delete", result.unwrapErr(), "OK");
                }
            }
        );
    }
};

TrashcanPopup* TrashcanPopup::create() {
    TrashcanPopup* popup = new TrashcanPopup();

    if (popup && popup->init()) {
        popup->autorelease();

        return popup;
    } else {
        delete popup;

        return nullptr;
    }
}

bool TrashcanPopup::init() {
    if (!Popup::init(350, 270)) return false;

    this->setTitle("Trashcan");

    constexpr CCSize SCROLL_SIZE = ccp(300, 180);
    CCSprite* trashcanSpr = CCSprite::createWithSpriteFrameName("edit_delBtn_001.png");
    CCLayerColor* scrollBG = CCLayerColor::create(ccc4(0, 0, 0, 90));
    ListBorders* border = ListBorders::create();
    CCSprite* deleteAllSpr = CCSprite::createWithSpriteFrameName("GJ_resetBtn_001.png");
    CCMenuItemSpriteExtra* deleteAllBtn = CCMenuItemSpriteExtra::create(deleteAllSpr, this, menu_selector(TrashcanPopup::onDeleteAll));
    m_scrollingLayer = ScrollLayer::create(SCROLL_SIZE);
    m_listener = UpdateTrashEvent().listen([this] {
        this->updateList();
    });

    trashcanSpr->setScale(0.7f);
    m_mainLayer->addChildAtPosition(trashcanSpr, Anchor::Top, ccp(-55, -20));

    scrollBG->setContentSize(SCROLL_SIZE + ccp(15, 15));
    scrollBG->ignoreAnchorPointForPosition(false);
    m_mainLayer->addChildAtPosition(scrollBG, Anchor::Center, ccp(0, 0), ccp(0.5f, 0.5f));

    m_scrollingLayer->m_contentLayer->setLayout(ScrollLayer::createDefaultListLayout());
    m_mainLayer->addChildAtPosition(m_scrollingLayer, Anchor::Center, -m_scrollingLayer->getContentSize() / 2);

    border->setContentSize(m_scrollingLayer->getContentSize() + ccp(15, 15));
    m_mainLayer->addChildAtPosition(border, Anchor::Center);

    m_buttonMenu->addChildAtPosition(deleteAllBtn, Anchor::BottomLeft, ccp(20, 20));

    this->updateList();

    return true;
}

void TrashcanPopup::updateList() {
    m_scrollingLayer->m_contentLayer->removeAllChildren();

    for (std::shared_ptr<TrashedItem> item : Trashcan::get()->getItems()) {
        m_scrollingLayer->m_contentLayer->addChild(TrashedItemNode::create(item));
    }

    m_scrollingLayer->m_contentLayer->updateLayout();
    m_scrollingLayer->moveToTop();

    // This is because updating the LevelBrowserLayer underneath causes it to take touch priority
    handleTouchPriority(this);
}

void TrashcanPopup::onDeleteAll(CCObject*) {
    createQuickPopup(
        "Clear Trashcan",
        fmt::format(
            "Are you sure you want to <co>clear the Trashcan</c>?\n"
            "<cr>This will PERMANENTLY delete ALL {} levels in the trash!</c>",
            Trashcan::get()->getItems().size()
        ),
        "Cancel", "Delete All",
        [this](FLAlertLayer*, const bool btn2) {
            if (!btn2) return;

            if (Result<> result = Trashcan::get()->deleteAllPermanently(); result.isErr()) {
                FLAlertLayer::create("Failed to Clear", result.unwrapErr(), "OK")->show();
            }

            this->onClose(nullptr);
        }
    );
}