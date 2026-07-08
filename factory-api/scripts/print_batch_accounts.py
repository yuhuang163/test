"""打印各工厂三类批量账号预览。"""
from app.database import SessionLocal
from app.seed import seed_factories
from app.services import user_batch

db = SessionLocal()
seed_factories(db)
data = user_batch.preview_batch_accounts(db, None, None)
print("工厂\t用户名\t密码\t角色")
for row in data["items"]:
    print(f"{row['factoryName']}\t{row['username']}\t{row['password']}\t{row['roles'][0]}")
print("---")
print(data["summary"])
