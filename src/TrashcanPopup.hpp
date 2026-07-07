#pragma once

#include "Trash.hpp"

class TrashcanPopup : public geode::Popup {
public:
    static TrashcanPopup* create();
private:
    geode::ScrollLayer* m_scrollingLayer;
    geode::ListenerHandle m_listener;

    bool init();
    void updateList();
    void onDeleteAll(CCObject* sender);
};
