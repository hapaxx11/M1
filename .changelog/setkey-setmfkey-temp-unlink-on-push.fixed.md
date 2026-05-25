**Sub-GHz Create-from-scratch: temp `.sub` no longer deleted on scene push** —
`SubGhzSceneSetKey` and `SubGhzSceneSetMfKey`'s `scene_on_exit()` handlers now
skip the defensive `unlink_tmp()` when `app->resume_from_child` is true. Without
this, pushing the Transmitter scene (which triggers our `on_exit` via
`subghz_scene_push()`) was deleting the temp `.sub` file before the Transmitter
could open it, so the very first one-press fire after entering the scene would
fail silently. The defensive unlink still runs on real exits (BACK,
`search_and_pop`).
