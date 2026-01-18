# Étape 1 : Build
FROM mcr.microsoft.com/dotnet/sdk:8.0 AS build
WORKDIR /src

# Copier le fichier projet et restaurer les dépendances
COPY ./src/*.csproj ./
RUN dotnet restore

# Copier le reste des fichiers et compiler
COPY ./src ./
RUN dotnet publish -c Release -o /app/publish

# Étape 2 : Runtime
FROM mcr.microsoft.com/dotnet/aspnet:8.0 AS runtime
WORKDIR /app
COPY --from=build /app/publish .

# Exposer le port par défaut d'ASP.NET Core
EXPOSE 8080
ENV ASPNETCORE_URLS=http://+:8080

ENTRYPOINT ["dotnet", "websocket.dll"]