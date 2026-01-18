namespace WebSocketApp.Models;

public class WsMessage
{
    public string? Type { get; set; }  
    public string? To { get; set; }    
    public object? Data { get; set; }  
}