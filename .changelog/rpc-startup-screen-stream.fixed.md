**RPC: prevent a startup freeze during home-screen rendering** — treat screen streaming as inactive until the routing mutex exists instead of waiting forever on an uninitialized mutex.
