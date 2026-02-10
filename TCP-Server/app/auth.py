"""
API key authentication middleware.
"""

from fastapi import Request, HTTPException
from starlette.middleware.base import BaseHTTPMiddleware


class APIKeyMiddleware(BaseHTTPMiddleware):
    """Middleware that checks X-API-Key header on /api/ routes."""

    def __init__(self, app, api_key: str):
        super().__init__(app)
        self.api_key = api_key

    async def dispatch(self, request: Request, call_next):
        # Only check API routes (skip health, ws, docs)
        if request.url.path.startswith("/api/"):
            key = request.headers.get("X-API-Key", "")
            if key != self.api_key:
                raise HTTPException(status_code=401, detail="Invalid or missing API key")

        return await call_next(request)
