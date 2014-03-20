/*
 * kevin_ai
 * A toy AI of wesnoth, use for test and familiar
*/

#include "../contexts.hpp"
#include "../interface.hpp"
#include "../composite/ai.hpp"
#include "../../pathfind/pathfind.hpp"

#include <boost/shared_ptr.hpp>

#ifndef REAL
#define REAL double
#endif

#ifdef _MSC_VER
#pragma warning(push)
//silence "inherits via dominance" warnings
#pragma warning(disable:4250)
#endif

namespace pathfind
{

struct plain_route;

} // of namespace pathfind


namespace ai
{

class kevin_ai : public ai_composite
{
public:
    kevin_ai(default_ai_context &context, const config& cfg);

    virtual void new_turn();
    virtual void play_turn();
    virtual void switch_side(side_number side);
    virtual std::string describe_self() const;
    virtual void on_create();

    virtual config to_config() const;

private:
    unit_map * units_;

protected:
    void do_attacks();  // the workhorse on attack analysis
};


} //end of namespace ai

#ifdef _MSC_VER
#pragma warning(pop)
#endif
