#include <Geode/Geode.hpp>
#include <Geode/modify/GJMessagePopup.hpp>

using namespace geode::prelude;

class $modify(MyGJMessagePopup, GJMessagePopup) {
	
	struct Fields {
    	std::unordered_map<int, EventListener<web::WebTask>> m_listeners;
	};

    void loadFromGJMessage(GJUserMessage* message) {
		GJMessagePopup::loadFromGJMessage(message);

		auto textArea = m_mainLayer->getChildByType<TextArea>(0);
		if (!textArea) return;
		auto bmf = textArea->getChildByType<MultilineBitmapFont>(0);
		if (!bmf) return;

		std::vector<Ref<CCFontSprite>> fontSprites;

		for (auto label : bmf->getChildrenExt()) {
			for (auto fontSprite : label->getChildrenExt()) {
				fontSprites.push_back(static_cast<CCFontSprite*>(fontSprite));
			}
		}
		
		const auto& content = message->m_content;
		processIDs(content, fontSprites, bmf);
	}

	void processIDs(const std::string& content, const std::vector<Ref<CCFontSprite>>& fontSprites, CCNode* parent) {
		int spriteIndex = 0;
		int contentIndex = 0;

		while (contentIndex < content.size()) {
			CCNode* sprParent = nullptr;
			if (!isdigit(content[contentIndex])) {
				contentIndex++;
				spriteIndex++;
				continue;
			}

			int startContent = contentIndex;
			int startSprite = spriteIndex;
			int value = 0;

			while (contentIndex < content.size() && isdigit(content[contentIndex])) {
				value = value * 10 + (content[contentIndex] - '0');
				contentIndex++;
				spriteIndex++;
			}

			int digitCount = contentIndex - startContent;
			if (digitCount < 3 && digitCount > 9)
				continue;

			CCPoint minPos, maxPos;
			bool first = true;

			for (int i = 0; i < digitCount; i++) {
				auto sprite = fontSprites[startSprite + i];
				sprite->setColor({0, 150, 255});
				if (!sprParent) sprParent = sprite->getParent();

				auto bounds = sprite->boundingBox();

				if (first) {
					minPos = CCPoint{bounds.getMinX(), bounds.getMinY()};
					maxPos = CCPoint{bounds.getMaxX(), bounds.getMaxY()};
					first = false;
				} else {
					minPos.x = std::min(minPos.x, bounds.getMinX());
					minPos.y = std::min(minPos.y, bounds.getMinY());
					maxPos.x = std::max(maxPos.x, bounds.getMaxX());
					maxPos.y = std::max(maxPos.y, bounds.getMaxY());
				}
			}

			auto menu = CCMenu::create();
			menu->ignoreAnchorPointForPosition(false);
			menu->setAnchorPoint({0, 0});
			menu->setContentSize(maxPos - minPos);

			auto worldPos = sprParent->convertToWorldSpace(minPos);
			auto nodePos = parent->convertToNodeSpace(worldPos);

			menu->setPosition(nodePos);
			parent->addChild(menu);

			auto btn = CCMenuItem::create(this, menu_selector(MyGJMessagePopup::onID));
			
			btn->setContentSize(maxPos - minPos);
			btn->setAnchorPoint({0, 0});
			btn->setPosition({0, 0});
			btn->setTag(value);

			menu->addChild(btn);
		}
	}

	void onID(CCObject* sender) {
		auto id = sender->getTag();
		auto fields = m_fields.self();

		auto popup = createQuickPopup("Getting Level", fmt::format("Searching for {}", id), "Cancel", nullptr, [fields, id] (auto alert, auto btn2) {
			if (!btn2) {
				fields->m_listeners.erase(id);
			}
		}, true, false);

		makeSearchFor(id, [self = Ref(this), popup = Ref(popup)] (GJGameLevel* level) {
			popup->removeFromParent();

			auto levelScene = LevelInfoLayer::scene(level, false);
			auto transitionFade = CCTransitionFade::create(0.5, levelScene);
			self->onClose(nullptr);
			CCDirector::sharedDirector()->pushScene(transitionFade);

		}, [popup = Ref(popup), id] {
			popup->removeFromParent();
			createQuickPopup("Oops", fmt::format("Failed to find {}", id), "OK", nullptr, nullptr);
		});
	}

	void makeSearchFor(int id, std::function<void(GJGameLevel*)> onLoad, std::function<void()> onFail) {

		CCObject* levelObject = GameLevelManager::get()->m_onlineLevels->objectForKey(numToString(id));
    	if (levelObject) {
			return onLoad(static_cast<GJGameLevel*>(levelObject));
		}

		auto fields = m_fields.self();
		fields->m_listeners[id].bind([this, onLoad, onFail, id] (web::WebTask::Event* e) {
			if (web::WebResponse* res = e->getValue()) {
				if (res->ok() && res->string().isOk()) {
					auto str = res->string().unwrap();
					if (str == "-1") return onFail();
					auto parts = utils::string::split(str, "#");
					auto levelsStr = parts[0];
					std::string creatorsStr = "";
    				if (parts.size() > 1) creatorsStr = parts[1];

					std::unordered_map<int, std::pair<std::string, int>> accountInformation;

					std::vector<std::string> creatorsData = utils::string::split(creatorsStr, "|");

					for (std::string creatorPart : creatorsData) {
						std::vector<std::string> creatorData = utils::string::split(creatorPart, ":");
						int userID = utils::numFromString<int>(creatorData[0]).unwrapOr(0);
						std::string userName = creatorData[1];
						int accountID = utils::numFromString<int>(creatorData[2]).unwrapOr(0);
						accountInformation[userID] = {userName, accountID};
					}

					auto levels = utils::string::split(levelsStr, "|");
					for (const auto& level : levels) {
						auto dict = GameLevelManager::responseToDict(level, false);
						auto gjLevel = GJGameLevel::create(dict, false);
						gjLevel->m_creatorName = accountInformation[gjLevel->m_userID].first;
						gjLevel->m_accountID = accountInformation[gjLevel->m_userID].second;
						onLoad(gjLevel);
						return;
					}

				}
				else {
					onFail();
				}
			}
		});

		auto req = web::WebRequest();
		req.bodyString(fmt::format("str={}&type=0&secret=Wmfd2893gb7", id));
		req.userAgent("");
		req.header("Content-Type", "application/x-www-form-urlencoded");
		fields->m_listeners[id].setFilter(req.post("http://www.boomlings.com/database/getGJLevels21.php"));
	}
};