#include "sc2api/sc2_api.h"

#include "sc2utils/sc2_manage_process.h"

#include <iostream>
#include <set>

const char* kReplayFolder = "C:/F2A/replays/";

float replay_average_mmr(const sc2::ReplayInfo& replay_info)
{
    float result = 0;
    int count = 0;
    for (int i = 0; i < replay_info.num_players; ++i)
    {
        if (replay_info.players[i].mmr > 0)//otherwise something's wrong. tolerate the error.
        {
            result += replay_info.players[i].mmr;
            ++count;
        }
    }
    if (count)
    {
        result /= count;
        return result;
    }
    return 0;
}

class Replay : public sc2::ReplayObserver {
public:
    std::vector<uint32_t> count_units_built_;

    std::set<sc2::Tag> previousUnits; // to cache and fix geyser scv bug

    Replay() :
        sc2::ReplayObserver() {
    }

    //! Determines if the replay should be filtered out.
    //! \param replay_info Replay information used to decide if the replay should be filtered.
    //!< \return If 'true', the replay will be rejected and not analyzed.
    virtual bool IgnoreReplay(const sc2::ReplayInfo& replay_info, uint32_t& player_id) override
    {
        bool ignore = sc2::ReplayObserver::IgnoreReplay(replay_info, player_id);
        bool game_unskilled = false;
        sc2::ReplayPlayerInfo info;
        if (replay_info.GetPlayerInfo(info, player_id))
        {
            game_unskilled = info.mmr > 0 && info.mmr < 3000; //there are occasions the player has 0 mmr. ignore that for now ...
        }

        if (game_unskilled)
        {
            std::cout << "Ignoring replay " << replay_info.replay_path << " because of low mmr " << info.mmr << "\n";
        }

        return ignore || game_unskilled;
    }

    void OnGameStart() final {
        std::cout << "average mmr: " << replay_average_mmr(this->ReplayControl()->GetReplayInfo()) << "\n";
        const sc2::ObservationInterface* obs = Observation();
        assert(obs->GetUnitTypeData().size() > 0);
        count_units_built_.resize(obs->GetUnitTypeData().size());
        std::fill(count_units_built_.begin(), count_units_built_.end(), 0);
    }

    void OnUnitCreated(const sc2::Unit* unit) final {
        assert(uint32_t(unit->unit_type) < count_units_built_.size());
        if (!previousUnits.count(unit->tag))//this filters out geyer worker that already exists
        {
            previousUnits.insert(unit->tag);
            ++count_units_built_[unit->unit_type];
            std::cout << "Units created = " << Observation()->GetUnitTypeData()[unit->unit_type].name
                << "\ttag = 0x" << std::hex << unit->tag
                << "\ttotal = " << std::dec << count_units_built_[unit->unit_type] << "\n";
        }
    }

    void OnStep() final {
    }

    void OnGameEnd() final {
        std::cout << "Units created:" << std::endl;
        const sc2::ObservationInterface* obs = Observation();
        const sc2::UnitTypes& unit_types = obs->GetUnitTypeData();
        for (uint32_t i = 0; i < count_units_built_.size(); ++i) {
            if (count_units_built_[i] == 0) {
                continue;
            }

            std::cout << unit_types[i].name << ": " << std::to_string(count_units_built_[i]) << std::endl;
        }
        std::cout << "Finished" << std::endl;
    }
};


int main(int argc, char* argv[]) {
    sc2::Coordinator coordinator;
    if (!coordinator.LoadSettings(argc, argv)) {
        return 1;
    }
    coordinator.SetRealtime(false);

    if (!coordinator.SetReplayPath(kReplayFolder)) {
        std::cout << "Unable to find replays." << std::endl;
        return 1;
    }

    Replay replay_observer;

    coordinator.AddReplayObserver(&replay_observer);

    coordinator.SetStepSize(100);

    while (coordinator.Update());
    while (!sc2::PollKeyPress());
}