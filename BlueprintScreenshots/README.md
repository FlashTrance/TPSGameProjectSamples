# Blueprint Screenshots
Just a few screenshots for some logic that's written in Blueprints. 95% of the logic for the game is done in C++, but there are times when I use Blueprints because it's siginificantly more intuitive. Most commonly, I use them for any logic that needs to be applied to HUD elements (widgets). Most of the Blueprint functions are defined as "BlueprintImplementableEvents" on the backend and called from C++.

I also use Blueprints for AnimNotifies; for example, the "GhostTrails"  screenshot in this folder shows an event on the player Character that is called by an AnimNotify.
