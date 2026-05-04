import { defineStore } from 'pinia'

interface UserState {
  token: string
  role: 'admin' | 'viewer' | ''
  username: string
}

export const useUserStore = defineStore('user', {
  state: (): UserState => ({
    token: localStorage.getItem('token') || '',
    role: (localStorage.getItem('role') as 'admin' | 'viewer' | '') || '',
    username: localStorage.getItem('username') || ''
  }),
  getters: {
    isLoggedIn: (state) => Boolean(state.token),
    isAdmin: (state) => state.role === 'admin',
    isViewer: (state) => state.role === 'viewer'
  },
  actions: {
    setAuth(token: string, role: 'admin' | 'viewer', username: string) {
      this.token = token
      this.role = role
      this.username = username
      localStorage.setItem('token', token)
      localStorage.setItem('role', role)
      localStorage.setItem('username', username)
    },
    logout() {
      this.token = ''
      this.role = ''
      this.username = ''
      localStorage.removeItem('token')
      localStorage.removeItem('role')
      localStorage.removeItem('username')
    }
  }
})
