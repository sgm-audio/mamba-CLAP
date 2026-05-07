from mastering_service.core.database import engine, SessionLocal
from mastering_service.core import models

def init_db():
    models.Base.metadata.create_all(bind=engine)
    
    db = SessionLocal()
    
    # Add default presets if they don't exist
    presets = [
        {
            "id": "transparent",
            "name": "Transparent",
            "description": "Clean, minimal coloration mastering focused on dynamic preservation.",
            "config_schema": {}
        },
        {
            "id": "warm",
            "name": "Warm",
            "description": "Analog-style saturation and gentle glue compression.",
            "config_schema": {}
        },
        {
            "id": "loud",
            "name": "Loud",
            "description": "Competitive streaming levels with transparent limiting.",
            "config_schema": {}
        }
    ]
    
    for p_data in presets:
        preset = db.query(models.Preset).filter(models.Preset.id == p_data["id"]).first()
        if not preset:
            preset = models.Preset(**p_data)
            db.add(preset)
    
    db.commit()
    db.close()

if __name__ == "__main__":
    print("Initializing database...")
    init_db()
    print("Database initialized.")
