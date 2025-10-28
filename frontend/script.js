class ChatApp {
    constructor() {
        this.chatMessages = document.getElementById('chat-messages');
        this.messageInput = document.getElementById('message-input');
        this.sendButton = document.getElementById('send-button');
        
        this.initEventListeners();
    }
    
    initEventListeners() {
        this.sendButton.addEventListener('click', () => this.sendMessage());
        this.messageInput.addEventListener('keypress', (e) => {
            if (e.key === 'Enter' && !e.shiftKey) {
                e.preventDefault();
                this.sendMessage();
            }
        });
    }
    
    async sendMessage() {
        const message = this.messageInput.value.trim();
        if (!message) return;
        
        this.addMessage('user', message);
        this.messageInput.value = '';
        this.setLoading(true);
        
        try {
            const response = await fetch('http://localhost:8080/api/chat', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({ message })
            });
            
            const data = await response.json();
            this.addMessage('bot', data.response);
        } catch (error) {
            this.addMessage('bot', 'Sorry, I encountered an error. Please make sure Ollama is running.');
            console.error('Error:', error);
        } finally {
            this.setLoading(false);
        }
    }
    
    addMessage(sender, text) {
        const messageDiv = document.createElement('div');
        messageDiv.className = `message ${sender}-message`;
        
        const contentDiv = document.createElement('div');
        contentDiv.className = 'message-content';
        contentDiv.textContent = text;
        
        messageDiv.appendChild(contentDiv);
        this.chatMessages.appendChild(messageDiv);
        this.scrollToBottom();
    }
    
    setLoading(loading) {
        this.sendButton.disabled = loading;
        this.sendButton.textContent = loading ? 'Sending...' : 'Send';
    }
    
    scrollToBottom() {
        this.chatMessages.scrollTop = this.chatMessages.scrollHeight;
    }
}

// Initialize chat app when page loads
document.addEventListener('DOMContentLoaded', () => {
    new ChatApp();
});