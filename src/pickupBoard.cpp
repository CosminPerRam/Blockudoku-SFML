
#include "pickupBoard.h"
#include "settings.h"
#include "audio.h"
#include "cellMatrix.h"

#include <SFML/Window/Event.hpp>
#include <SFML/Window/Mouse.hpp>

#include <iostream>

PickupBoard::PickupBoard(Table& theTable, Score& theScore) : theTable(theTable), theScore(theScore)
{
    PickupBoard::generateBlocks();
}

PickupBoard::~PickupBoard()
{
    for (unsigned i = 0; i < 3; i++)
        delete pickupableBlocks[i];
}

void PickupBoard::reset() {
    PickupBoard::generateBlocks();
}

void PickupBoard::calculateVertexes() {
    for (unsigned i = 0; i < 2; i++) {
        float rowY = BoardHeight * i * 1.f;
        borders[i * 2].position = { startPosition.y, rowY + startPosition.x };
        borders[i * 2].color = MARGINS;
        borders[i * 2 + 1].position = { BoardLength + startPosition.y, rowY + startPosition.x };
        borders[i * 2 + 1].color = MARGINS;
    }

    unsigned gridPositionOffset = 4;
    for (unsigned i = 0; i < 4; i++) {
        float rowX = BoardHeight * i * 1.f;
        borders[gridPositionOffset + i * 2].position = { rowX + startPosition.y, startPosition.x };
        borders[gridPositionOffset + i * 2].color = MARGINS;
        borders[gridPositionOffset + i * 2 + 1].position = { rowX + startPosition.y, BoardHeight + startPosition.x };
        borders[gridPositionOffset + i * 2 + 1].color = MARGINS;
    }
}

void PickupBoard::generateBlocks() {
    for (int i = 0; i < 3; i++) {
        pickupableBlocks[i] = new Block(Settings::Gameplay::blockModel);
        PickupBoard::placeIndexInDefaultPosition(i);
    }
}

void PickupBoard::placeIndexInDefaultPosition(unsigned i) {
    pickupableBlocks[i]->setScale(0.85f + (3 - (int)std::max(pickupableBlocks[i]->getStructureSize().x, pickupableBlocks[i]->getStructureSize().y)) * 0.15f);

    sf::FloatRect bounds = pickupableBlocks[i]->getGlobalBounds();
    pickupableBlocks[i]->setPosition({ PICKUP_POSITION_Y + BoardHeight * i + (BoardHeight - bounds.width) / 2.f,
        PICKUP_POSITION_X + (BoardHeight - bounds.height) / 2.f });
}

bool PickupBoard::anyBlocksLeft() {
	for (unsigned i = 0; i < 3; i++) {
		if (pickupableBlocks[i] != nullptr)
			return true;
	}

	return false;
}

bool PickupBoard::canAnyBlocksBePlaced() {
    bool canAnyBePlaced = false;

    for (unsigned i = 0; i < 3; i++) {
        if (pickupableBlocks[i] == nullptr)
            continue;

        if (theTable.canBlockBePlaced(*pickupableBlocks[i])) {
            canAnyBePlaced = true;
            pickupableBlocks[i]->setOpacity(255);
        }
        else
            pickupableBlocks[i]->setOpacity(128);
    }

    return canAnyBePlaced;
}

bool PickupBoard::recursiveCanAnyBlocksBePlaced(int firstIndex, int secondIndex)
{
    static cellMatrix virtualMatrix;

    if (firstIndex == -1)
        virtualMatrix = theTable.getMatrix();

    unsigned amountSkipped = 0;
    for (unsigned i = 0; i < 3; i++) {
        Block* theCurrentBlock = pickupableBlocks[i];

        if (theCurrentBlock == nullptr || i == firstIndex || i == secondIndex) {
            amountSkipped++;

            if (amountSkipped == 3)
                return true;
        }
        else
        {
            auto positions = virtualMatrix.getBlockPlacingPositions(*theCurrentBlock);

            std::unique_ptr<std::vector<completetion>> positionCompletetions;
            for (const auto& p : positions) {
                virtualMatrix.applyBlock(*theCurrentBlock, p);
                positionCompletetions = virtualMatrix.checkCompletetion();
                virtualMatrix.executeCompletetions(positionCompletetions, cell::empty);

                if (firstIndex == -1) {
                    if (recursiveCanAnyBlocksBePlaced(i))
                        return true;
                }
                else {
                    if (secondIndex != 1 || recursiveCanAnyBlocksBePlaced(firstIndex, i))
                        return true;
                }

                virtualMatrix.executeCompletetions(positionCompletetions, cell::occupied);
                virtualMatrix.applyBlock(*theCurrentBlock, p, cell::empty);
            }
        }
    }

    return false;
}

void PickupBoard::draw(sf::RenderWindow& window)
{
    window.draw(borders);

    for (unsigned i = 0; i < 3; i++) {
        if (pickupableBlocks[i] != nullptr)
            pickupableBlocks[i]->draw(window);
    }
}

void PickupBoard::pollEvent(sf::RenderWindow& window, sf::Event& theEvent)
{
    sf::Vector2f mousePosition = window.mapPixelToCoords(sf::Mouse::getPosition(window));
    sf::Vector2f mousePositionWithOffset = { mousePosition.x - PICKUP_OFFSET, mousePosition.y - PICKUP_OFFSET };

    if (pickedUpIndex != -1) {
        pickupableBlocks[pickedUpIndex]->setPosition(mousePositionWithOffset);
        pickedUpPreviewCoords = theTable.previewBlock(*pickupableBlocks[pickedUpIndex], mousePosition);
    }
    else {
        if (theEvent.type == sf::Event::MouseButtonPressed && theEvent.mouseButton.button == sf::Mouse::Left) {
            for (unsigned i = 0; i < 3 && pickedUpIndex == -1; i++) { //check for every 3 blocks from the pickup area
                if (pickupableBlocks[i] != nullptr) { //if they are picked up, if a block is nullptr, it means it was used
                    if (pickupableBlocks[i]->getGlobalBounds().contains({ mousePosition.x, mousePosition.y }))
                    {
                        pickedUpIndex = i;

                        pickupableBlocks[i]->setFloating(true);
                        pickupableBlocks[i]->setScale(PICKUP_SCALE);
                    }
                }
            }
        }
    }
    
    if (theEvent.type == sf::Event::MouseButtonReleased && theEvent.mouseButton.button == sf::Mouse::Left) {
        if (pickedUpIndex != -1)
        {
            pickupableBlocks[pickedUpIndex]->setFloating(false);

            if (pickedUpPreviewCoords.x != -1 || pickedUpPreviewCoords.y != -1) {
                theTable.applyBlock(*pickupableBlocks[pickedUpIndex], {static_cast<unsigned>(pickedUpPreviewCoords.x), static_cast<unsigned>(pickedUpPreviewCoords.y)});

                theScore.addPiecePlaced(pickupableBlocks[pickedUpIndex]->getStructureIndex());

                delete pickupableBlocks[pickedUpIndex];

                if (Settings::Gameplay::continousGenerate) {
                    pickupableBlocks[pickedUpIndex] = new Block(Settings::Gameplay::blockModel);
                    PickupBoard::placeIndexInDefaultPosition(pickedUpIndex);
                }
                else
                    pickupableBlocks[pickedUpIndex] = nullptr;

                if (!anyBlocksLeft())
                    generateBlocks();

                std::cout << recursiveCanAnyBlocksBePlaced() << std::endl << std::endl;
                /*if (Settings::Gameplay::checkGameInAdvance)
                {
                    if (!canAnyBlocksBePlaced() || !recursiveCanAnyBlocksBePlaced())
                        theScore.setGameLost();
                }
                else */{
                    if (!canAnyBlocksBePlaced())
                        theScore.setGameLost();
                }

                pickedUpPreviewCoords = { -1, -1 };
            }
            else {
                placeIndexInDefaultPosition(pickedUpIndex);

                Audio::play(Audio::effect::BadPlacement);
            }

            pickedUpIndex = -1;
        }
    }
}

void PickupBoard::updateColors() {
    MARGINS = toColor(Settings::Aspect::pickupMargins);

    PickupBoard::calculateVertexes();
}