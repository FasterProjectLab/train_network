FROM alpine:latest

RUN apk add --no-cache hostapd dnsmasq iw gettext

WORKDIR /app
COPY . .

RUN chmod +x entrypoint.sh

ENTRYPOINT ["./entrypoint.sh"]