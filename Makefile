# Convenience entry points. Real work is app/build.sh (WSL / Linux).
.PHONY: doctor app deploy

doctor:
	bash scripts/doctor.sh

app:
	bash app/build.sh

deploy:
	bash app/build.sh deploy $(PS5_HOST)
