FROM nginx:alpine

RUN apk add --no-cache python3 spawn-fcgi fcgiwrap

RUN mkdir -p /var/run \
    && chown nginx:nginx /var/run \
    && chmod 755 /var/run

RUN mkdir -p /var/cache/nginx/client_temp \
	&& chown -R nginx:nginx /var/cache/nginx

USER nginx

CMD spawn-fcgi -s /var/run/fcgiwrap.socket /usr/bin/fcgiwrap && \
	nginx -g 'daemon off;'
