"""
Main application factory for the AI-Powered Audio Mastering Service.
"""

import os
import traceback

from flask import Flask, jsonify
from flask_cors import CORS
from flask_jwt_extended import JWTManager
from flask_marshmallow import Marshmallow
from flask_sqlalchemy import SQLAlchemy
from redis import Redis
from rq import Queue

# Initialize extensions
db = SQLAlchemy()
ma = Marshmallow()
jwt = JWTManager()
cors = CORS()


def create_app(config_name=None):
    """
    Application factory pattern.
    :param config_name: Configuration to use (development, testing, production)
    :return: Flask app instance
    """
    app = Flask(__name__, instance_relative_config=False)

    # Load configuration
    if config_name is None:
        config_name = os.environ.get("FLASK_ENV", "development")

    from backend.config import config_dict

    app.config.from_object(config_dict[config_name])

    # Initialize extensions with app
    db.init_app(app)
    ma.init_app(app)
    jwt.init_app(app)
    cors.init_app(app)

    # Initialize Redis connection for RQ
    app.redis = Redis.from_url(app.config["REDIS_URL"])
    app.task_queue = Queue("mastering-tasks", connection=app.redis)

    # Set up JWT token blacklist checking
    if app.config.get("JWT_TOKEN_BLACKLIST_ENABLED", False):

        @jwt.token_in_blocklist_loader
        def check_if_token_in_blocklist(decoded_token):
            jti = decoded_token.get("jti")
            if not jti:
                return True  # Invalid token if no JTI
            blocklisted = app.redis.get(f"jwt_blocklist:{jti}")
            return blocklisted is not None

    # Register blueprints
    from backend.api.v1 import api_bp

    app.register_blueprint(api_bp, url_prefix="/api/v1")

    # Register global error handlers
    @app.errorhandler(400)
    def bad_request(error):
        return jsonify({"error": "BAD_REQUEST", "message": str(error)}), 400

    @app.errorhandler(404)
    def not_found(error):
        return jsonify({"error": "NOT_FOUND", "message": "Resource not found"}), 404

    @app.errorhandler(422)
    def unprocessable_entity(error):
        return jsonify({"error": "UNPROCESSABLE_ENTITY", "message": str(error)}), 422

    @app.errorhandler(500)
    def internal_error(error):
        app.logger.error(f"Internal server error: {traceback.format_exc()}")
        return (
            jsonify(
                {
                    "error": "INTERNAL_ERROR",
                    "message": (
                        "An unexpected error occurred"
                        if app.config.get("DEBUG")
                        else "Internal server error"
                    ),
                }
            ),
            500,
        )

    # Create database tables if they don't exist
    with app.app_context():
        db.create_all()

    return app
